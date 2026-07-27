local rime_dir = assert(arg[1], "Rime directory is required")

rime_api = {
  get_user_data_dir = function()
    return rime_dir
  end,
  get_shared_data_dir = function()
    return rime_dir
  end,
}

local callbacks = {}
local function notifier(name)
  return {
    connect = function(_, callback)
      callbacks[name] = callback
      return { disconnect = function() end }
    end,
  }
end

local options = {
  smart_english = true,
  punct_auto = true,
}
local properties = {}
local candidates = {}
local commits = {}

local segment = {
  menu = {
    candidate_count = function()
      return #candidates
    end,
  },
  get_candidate_at = function(_, index)
    return candidates[index + 1]
  end,
}

local context
context = {
  input = "",
  commit_notifier = notifier("commit"),
  update_notifier = notifier("update"),
  composition = {
    back = function()
      return segment
    end,
  },
  get_option = function(_, name)
    return options[name] or false
  end,
  set_option = function(_, name, value)
    options[name] = value
  end,
  get_property = function(_, name)
    return properties[name] or ""
  end,
  set_property = function(_, name, value)
    properties[name] = value
  end,
  is_composing = function()
    return context.input ~= ""
  end,
  has_menu = function()
    return #candidates > 0
  end,
  clear = function()
    context.input = ""
    candidates = {}
  end,
  get_commit_text = function()
    return commits[#commits] or ""
  end,
}

local config = {
  get_string = function()
    return nil
  end,
  get_int = function()
    return nil
  end,
  get_double = function()
    return nil
  end,
}

local engine = {
  context = context,
  schema = { config = config },
  commit_text = function(_, text)
    commits[#commits + 1] = text
  end,
}

local module = require("smart_english_commit")
local env = { engine = engine }
module.init(env)

local function key(name)
  return { repr = function() return name end }
end

context.input = "doing"
assert(module.func(key("space"), env) == 1)
assert(commits[#commits] == "doing ", "known English should commit immediately")

context.input = "sucl"
candidates = { { text = "你好" } }
assert(module.func(key("space"), env) == 2)
callbacks.update(context)
assert(commits[#commits] == "doing ", "first-tone Chinese candidate should win")

context.input = "sucl"
candidates = { { text = "你好" } }
assert(module.func(key("space"), env) == 2)
candidates = {}
callbacks.update(context)
assert(commits[#commits] == "sucl ", "no first-tone Chinese candidate should fall back to English")

context.input = "sucl"
candidates = {}
assert(module.func(key("Return"), env) == 1)
assert(commits[#commits] == "sucl", "Enter without Chinese candidates should commit English")

properties.smart_pending_space = ""
context.input = "su3"
candidates = { { text = "你" } }
assert(module.func(key("space"), env) == 2)
assert(properties.smart_pending_space == "", "toned Zhuyin must not arm English fallback")
assert(module.func(key("Return"), env) == 2)
assert(commits[#commits] == "sucl", "toned Zhuyin must be committed by Rime")

module.fini(env)
print("Lua smart-English commit tests passed")
