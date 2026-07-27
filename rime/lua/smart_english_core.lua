local M = {}

local zhuyin_category = {}
for key in ("1qaz2wsxedcrfv5tgbyhn"):gmatch(".") do
  zhuyin_category[key] = 1 -- 聲母
end
for key in ("ujm"):gmatch(".") do
  zhuyin_category[key] = 2 -- 介音
end
for key in ("ik,9ol.0p;/-8"):gmatch(".") do
  zhuyin_category[key] = 3 -- 韻母
end

local function normalize(line)
  return (line:gsub("\r$", ""))
end

function M.load_dictionary(path)
  local words = {}
  local prefixes = {}
  local file = io.open(path, "r")
  if not file then
    return words, prefixes, false
  end

  for raw_line in file:lines() do
    local line = normalize(raw_line)
    if line ~= "" and not line:match("^#") then
      local word, raw_weight = line:match("^([%w][%w%._%+%-']*)%s+([0-9]+)$")
      if word then
        word = word:lower()
        local weight = tonumber(raw_weight) or 100
        words[word] = math.max(words[word] or 0, weight)
        for length = 2, #word - 1 do
          local prefix = word:sub(1, length)
          prefixes[prefix] = math.max(prefixes[prefix] or 0, weight)
        end
      end
    end
  end

  file:close()
  return words, prefixes, true
end

local function has_english_shape(input, min_length)
  if #input < min_length then
    return false, nil
  end

  if input:match("[_%-]") then
    return true, "token"
  end
  if input:match("%a%d") or input:match("%d%a") then
    return true, "token"
  end
  if input:match("^[a-z]+%.?[a-z]+%.[a-z][a-z]+$") then
    return true, "domain"
  end

  local lower = input:lower()
  local suffixes = {
    "ing", "tion", "sion", "ment", "ness", "able", "ible",
    "ous", "ful", "less", "ize", "ise", "ed", "ly",
  }
  for _, suffix in ipairs(suffixes) do
    if lower:sub(-#suffix) == suffix then
      return true, "suffix"
    end
  end

  return false, nil
end

local function edit_distance_at_most_one(left, right)
  local left_length = #left
  local right_length = #right
  if math.abs(left_length - right_length) > 1 then
    return false
  end

  if left_length == right_length then
    local differences = 0
    for index = 1, left_length do
      if left:sub(index, index) ~= right:sub(index, index) then
        differences = differences + 1
        if differences > 1 then
          return false
        end
      end
    end
    return differences == 1
  end

  local shorter, longer = left, right
  if left_length > right_length then
    shorter, longer = right, left
  end
  local short_index, long_index, skipped = 1, 1, false
  while short_index <= #shorter and long_index <= #longer do
    if shorter:sub(short_index, short_index) == longer:sub(long_index, long_index) then
      short_index = short_index + 1
      long_index = long_index + 1
    elseif skipped then
      return false
    else
      skipped = true
      long_index = long_index + 1
    end
  end
  return true
end

local function fuzzy_dictionary_match(input, words, minimum_weight)
  local best_word = nil
  local best_weight = 0
  for word, weight in pairs(words) do
    if weight >= minimum_weight and edit_distance_at_most_one(input, word) then
      if weight > best_weight then
        best_word = word
        best_weight = weight
      end
    end
  end
  return best_word, best_weight
end

local function minimum_zhuyin_syllables(input)
  local length = #input
  local best = { [0] = 0 }
  for finish = 1, length do
    local minimum = math.huge
    for chunk_length = 1, 3 do
      local start = finish - chunk_length + 1
      if start >= 1 and best[start - 1] then
        local previous_category = 0
        local valid = true
        for index = start, finish do
          local category = zhuyin_category[input:sub(index, index)]
          if not category or category <= previous_category then
            valid = false
            break
          end
          previous_category = category
        end
        if valid then
          minimum = math.min(minimum, best[start - 1] + 1)
        end
      end
    end
    if minimum < math.huge then
      best[finish] = minimum
    end
  end
  return best[length] or length
end

local function structurally_unlikely_zhuyin(input, minimum_length, threshold)
  if #input < minimum_length or not input:match("^[a-z]+$") then
    return false, 0
  end
  local syllables = minimum_zhuyin_syllables(input)
  local fragmentation = syllables / #input
  return fragmentation > threshold, fragmentation
end

local function is_structured_zhuyin_with_tones(input)
  if not input:match("[3467]") then
    return false
  end

  local chunk = ""
  local saw_tone = false
  for index = 1, #input do
    local character = input:sub(index, index)
    if character:match("[3467]") then
      if chunk == "" or minimum_zhuyin_syllables(chunk) ~= 1 then
        return false
      end
      chunk = ""
      saw_tone = true
    else
      chunk = chunk .. character
    end
  end

  if chunk ~= "" and minimum_zhuyin_syllables(chunk) ~= 1 then
    return false
  end
  return saw_tone
end

function M.classify(input, words, prefixes, options)
  options = options or {}
  local exact_threshold = options.exact_threshold or 100
  local prefix_threshold = options.prefix_threshold or 250
  local prefix_min_length = options.prefix_min_length or 3
  local heuristic_min_length = options.heuristic_min_length or 5
  local structural_min_length = options.structural_min_length or 4
  local structural_threshold = options.structural_threshold or 0.50
  local fuzzy_min_length = options.fuzzy_min_length or 4
  local fuzzy_min_weight = options.fuzzy_min_weight or 250

  if type(input) ~= "string" or input == "" then
    return { english = false, reason = "empty", score = 0 }
  end

  if not input:match("^[A-Za-z][A-Za-z0-9%._%+%-']*$") then
    return { english = false, reason = "bopomofo", score = 0 }
  end

  local lower = input:lower()
  if is_structured_zhuyin_with_tones(lower) then
    return { english = false, reason = "bopomofo", score = 0 }
  end

  local weight = words[lower] or 0
  if weight >= exact_threshold then
    return { english = true, exact = true, reason = "dictionary", score = weight }
  end

  if #lower >= fuzzy_min_length then
    local correction, correction_weight =
      fuzzy_dictionary_match(lower, words, fuzzy_min_weight)
    if correction then
      return {
        english = true,
        exact = false,
        reason = "fuzzy",
        score = math.min(199, math.floor(correction_weight / 3)),
        correction = correction,
      }
    end
  end

  local shaped, shape_reason = has_english_shape(input, heuristic_min_length)
  if shaped then
    return { english = true, exact = false, reason = shape_reason, score = 90 }
  end

  local prefix_weight = prefixes[lower] or 0
  if #lower >= prefix_min_length and prefix_weight >= prefix_threshold then
    return {
      english = true,
      exact = false,
      prefix = true,
      reason = "prefix",
      score = math.min(99, math.floor(prefix_weight / 4)),
    }
  end

  local structurally_english, fragmentation =
    structurally_unlikely_zhuyin(
      lower,
      structural_min_length,
      structural_threshold
    )
  if structurally_english then
    return {
      english = true,
      exact = false,
      reason = "zhuyin_structure",
      score = math.floor(100 + fragmentation * 50),
    }
  end

  return { english = false, reason = "uncertain", score = weight }
end

function M.text_language(text)
  if type(text) ~= "string" or text == "" then
    return nil
  end

  local has_latin = false
  for _, codepoint in utf8.codes(text) do
    if
      (codepoint >= 0x3400 and codepoint <= 0x4DBF)
      or (codepoint >= 0x4E00 and codepoint <= 0x9FFF)
      or (codepoint >= 0xF900 and codepoint <= 0xFAFF)
      or (codepoint >= 0x20000 and codepoint <= 0x323AF)
    then
      return "zh"
    end
    if
      (codepoint >= 0x41 and codepoint <= 0x5A)
      or (codepoint >= 0x61 and codepoint <= 0x7A)
      or (codepoint >= 0x30 and codepoint <= 0x39)
    then
      has_latin = true
    end
  end
  return has_latin and "en" or nil
end

return M
