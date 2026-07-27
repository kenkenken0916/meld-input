local core = require("smart_english_core")

local function config_number(config, key, fallback)
  local value = config:get_int(key)
  return value or fallback
end

local function find_dictionary(filename)
  local user_path = rime_api.get_user_data_dir() .. "/" .. filename
  local probe = io.open(user_path, "r")
  if probe then
    probe:close()
    return user_path
  end

  local shared_path = rime_api.get_shared_data_dir() .. "/" .. filename
  probe = io.open(shared_path, "r")
  if probe then
    probe:close()
    return shared_path
  end
  return user_path
end

local M = {}

local function has_chinese_candidate(context)
  if not context:has_menu() then
    return false
  end

  local segment = context.composition:back()
  if not segment or not segment.menu then
    return false
  end

  local count = math.min(segment.menu:candidate_count(), 20)
  for index = 0, count - 1 do
    local candidate = segment:get_candidate_at(index)
    if candidate and core.text_language(candidate.text) == "zh" then
      return true
    end
  end
  return false
end

local function apply_punctuation_mode(context)
  if context:get_option("punct_en") then
    context:set_option("ascii_punct", true)
  elseif context:get_option("punct_zh") then
    context:set_option("ascii_punct", false)
  elseif context:get_option("punct_auto") then
    context:set_option(
      "ascii_punct",
      context:get_property("smart_last_language") == "en"
    )
  end
end

function M.init(env)
  local config = env.engine.schema.config
  local base = "smart_english"
  local dictionary = config:get_string(base .. "/dictionary") or "smart_english.tsv"
  env.engine.context:set_option("taiwan_fixed", true)
  env.engine.context:set_option("full_shape", false)

  env.options = {
    exact_threshold = config_number(config, base .. "/exact_threshold", 100),
    prefix_min_length = config_number(config, base .. "/prefix_min_length", 3),
    prefix_threshold = config_number(config, base .. "/prefix_threshold", 250),
    heuristic_min_length = config_number(config, base .. "/heuristic_min_length", 5),
    structural_min_length = config_number(config, base .. "/structural_min_length", 4),
    structural_threshold =
      config:get_double(base .. "/structural_threshold") or 0.50,
    fuzzy_min_length = config_number(config, base .. "/fuzzy_min_length", 4),
    fuzzy_min_weight = config_number(config, base .. "/fuzzy_min_weight", 250),
  }
  env.auto_commit_min_length =
    config_number(config, base .. "/auto_commit_min_length", 3)
  env.words, env.prefixes =
    core.load_dictionary(find_dictionary(dictionary))

  env.commit_connection = env.engine.context.commit_notifier:connect(
    function(context)
      local language = core.text_language(context:get_commit_text())
      if language then
        context:set_property("smart_last_language", language)
      end
    end
  )
  env.update_connection = env.engine.context.update_notifier:connect(
    function(context)
      local pending = context:get_property("smart_pending_space")
      if not pending or pending == "" then
        return
      end

      context:set_property("smart_pending_space", "")
      if has_chinese_candidate(context) then
        return
      end

      env.engine:commit_text(pending .. " ")
      context:set_property("smart_last_language", "en")
      context:clear()
    end
  )
  apply_punctuation_mode(env.engine.context)
end

function M.fini(env)
  if env.commit_connection then
    env.commit_connection:disconnect()
  end
  if env.update_connection then
    env.update_connection:disconnect()
  end
end

function M.func(key, env)
  apply_punctuation_mode(env.engine.context)

  local key_name = key:repr()
  if key_name ~= "space" and key_name ~= "Return" and key_name ~= "KP_Enter" then
    return 2
  end

  local context = env.engine.context
  if not context:get_option("smart_english") or not context:is_composing() then
    return 2
  end

  local input = context.input
  if #input < env.auto_commit_min_length then
    return 2
  end

  local result = core.classify(input, env.words, env.prefixes, env.options)
  -- 已含合法聲調的注音，空白是確認候選、Enter 是提交組字；
  -- 不可再掛上英文 fallback，否則 Rime 清空 menu 後會被誤判成無中文候選。
  if result.reason == "bopomofo" then
    return 2
  end

  local raw_latin_word = input:match("^[A-Za-z][A-Za-z0-9%._%+%-']*$") ~= nil
  if not raw_latin_word then
    return 2
  end

  if key_name == "Return" or key_name == "KP_Enter" then
    if result.english or not has_chinese_candidate(context) then
      env.engine:commit_text(input)
      context:set_property("smart_last_language", "en")
      context:clear()
      return 1
    end
    return 2
  end

  if result.english then
    env.engine:commit_text(input .. " ")
    context:set_property("smart_last_language", "en")
    context:clear()
    return 1
  end

  -- 模稜兩可時不搶走空白鍵。先讓 speller 把它當一聲，
  -- update_notifier 會在候選重算後決定保留中文或回退英文。
  context:set_property("smart_pending_space", input)
  return 2
end

return M
