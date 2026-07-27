local dictionary_path = assert(arg[1], "dictionary path is required")
local core = require("smart_english_core")
local words, prefixes, loaded = core.load_dictionary(dictionary_path)
assert(loaded, "dictionary should load")

local options = {
  exact_threshold = 100,
  prefix_min_length = 3,
  prefix_threshold = 250,
  heuristic_min_length = 5,
  structural_min_length = 4,
  structural_threshold = 0.50,
  fuzzy_min_length = 4,
  fuzzy_min_weight = 250,
}

local function expect_english(input, expected, reason)
  local result = core.classify(input, words, prefixes, options)
  assert(
    result.english == expected,
    string.format("%s: expected english=%s, got %s (%s)", input, expected, result.english, result.reason)
  )
  if reason then
    assert(result.reason == reason, input .. ": unexpected reason " .. result.reason)
  end
end

expect_english("linux", true, "dictionary")
expect_english("hello", true, "dictionary")
expect_english("config", true, "dictionary")
expect_english("hel", true, "prefix")
expect_english("stuft", true, "fuzzy")
expect_english("unknowning", true, "suffix")
expect_english("hello-world", true, "token")
expect_english("abc123", true, "token")
expect_english("dfgh", true, "zhuyin_structure")
expect_english("su3", false, "bopomofo")
expect_english("su3cl", false, "bopomofo")
expect_english("su3cl3", false, "bopomofo")
expect_english("1qaz", false, "bopomofo")
expect_english("sucl", false, "uncertain")
expect_english("", false, "empty")

local typo = core.classify("stuft", words, prefixes, options)
assert(typo.correction == "stuff", "stuft should suggest stuff")
assert(core.text_language("hello") == "en", "hello should be English")
assert(core.text_language("你好") == "zh", "你好 should be Chinese")
assert(core.text_language("。") == nil, "punctuation should not replace language")

print("Lua smart-English tests passed")
