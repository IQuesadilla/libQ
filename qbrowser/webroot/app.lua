local dark_mode = false
local text_color = dark_mode and
    { r = 255, g = 255, b = 255 } or
    { r = 0, g = 0, b = 0 }

local win_radius = 32
local mdown_old = false

window.draw = function()
  window.item {
    name = "root",
    layout = {
      sizing = {
        width = window.sizing_grow(0),
        height = window.sizing_grow(0),
      },
      layoutDirection = 1,
    },
    backgroundColor = { r = 0, g = 0, b = 0, a = 0 },
    drawchildren = function()
      local header_hovered = window.is_hovered("header")
      window.drag = header_hovered and window.mdown
      window.item {
        name = "header",
        layout = {
          sizing = {
            width = window.sizing_grow(0),
            height = window.sizing_fixed(50),
          },
          padding = { t = 6, b = 6, r = 24, l = 24 },
        },
        cornerRadius = { tl = win_radius, tr = win_radius },
        backgroundColor = { r = 240, g = 240, b = 240 },
        drawchildren = function()
          local open_hovered = window.is_hovered("open")
          local open_pressed = open_hovered and not window.mdown and mdown_old
          if open_pressed then
            print("pressed")
          end
          window.drag = window.drag and not open_hovered

          window.item {
            name = "open",
            layout = {
              padding = { t = 8, b = 8, r = 8, l = 8 },
              childGap = 6,
            },
            backgroundColor = open_hovered and
                { r = 220, g = 220, b = 220 } or
                { r = 220, g = 220, b = 220, a = 0 },
            cornerRadius = { tl = 8 },
            drawchildren = function()
              window.text {
                text = "Open",
                fontSize = 16,
                textColor = text_color,
              }
              window.item {
                name = "go-icon",
                image = "assets/arrow-email-forward.png",
                layout = {
                  sizing = {
                    width = window.sizing_fixed(18),
                    height = window.sizing_fixed(18),
                  },
                },
              }
            end
          }

          window.item {
            name = "spacer",
            layout = {
              sizing = {
                width = window.sizing_grow(0),
                height = window.sizing_grow(0),
              },
            },
          }

          local exit_hovered = window.is_hovered("exit")
          if exit_hovered and not window.mdown and mdown_old then
            window.close(0)
          end
          window.drag = window.drag and not exit_hovered

          window.item {
            name = "exit",
            layout = {
              padding = { t = 3, b = 3, r = 6, l = 6 },
              sizing = {
                width = window.sizing_fixed(25),
                height = window.sizing_fixed(25),
              }
            },
            backgroundColor = exit_hovered and
                { r = 230, g = 20, b = 20 } or
                { r = 150, g = 20, b = 20 },
            cornerRadius = { tl = 6 },
            image = exit_hovered and
                "assets/xmark-circle.png" or
                "assets/xmark-circle-solid.png"
          }
        end
      }

      window.item {
        name = "body",
        layout = {
          sizing = {
            width = window.sizing_grow(0),
            height = window.sizing_grow(0),
          },
          padding = { t = 4, b = 4, r = 4, l = 4 },
        },
        cornerRadius = { bl = win_radius, br = win_radius },
        backgroundColor = { r = 255, g = 255, b = 255, a = 150 },
      }
    end
  }
  mdown_old = window.mdown
end
