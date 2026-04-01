---@class Color
local BG = {
  r = 70,
  g = 70,
  b = 120,
  a = 255,
}

local count = 0

window.redraw = function()
  local width = window.sizing_fixed(300)
  window.draw {
    name = "outer",
    layout = {
      sizing = {
        width = width,
        height = window.sizing_fixed(30),
      },
    },
    backgroundColor = BG,
    handler = function()
      window.text(tostring(count))
      count = count + 1

      window.draw {
        x = 3,
        y = 8,
        name = "inner1",
      }

      window.draw {
        x = 4,
        y = 9,
        name = "inner2",
      }
    end
  }
end
