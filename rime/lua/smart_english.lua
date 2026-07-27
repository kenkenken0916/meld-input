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

function M.init(env)
  local config = env.engine.schema.config
  local base = "smart_english"
  local dictionary = config:get_string(base .. "/dictionary") or "smart_english.tsv"

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
  env.show_prefix = config:get_bool(base .. "/show_prefix_candidate")
  env.comment = config:get_string(base .. "/comment") or "〔EN〕"
  env.words, env.prefixes, env.dictionary_loaded =
    core.load_dictionary(find_dictionary(dictionary))

  if not env.dictionary_loaded then
    log.warning("smart_english: dictionary not found: " .. dictionary)
  end
end

function M.func(input, seg, env)
  if not env.engine.context:get_option("smart_english") then
    return
  end

  local result = core.classify(input, env.words, env.prefixes, env.options)
  if not result.english then
    return
  end
  if result.prefix and not env.show_prefix then
    return
  end

  local comment = result.prefix and "〔EN…〕" or env.comment
  local candidate = Candidate("smart_english", seg.start, seg._end, input, comment)
  candidate.preedit = input
  candidate.quality = result.exact and (1000 + result.score) or (100 + result.score)
  yield(candidate)

  if result.correction and result.correction ~= input then
    local correction = Candidate(
      "smart_english_correction",
      seg.start,
      seg._end,
      result.correction,
      "〔EN 修正〕"
    )
    correction.preedit = input
    correction.quality = 90 + result.score
    yield(correction)
  end
end

return M
