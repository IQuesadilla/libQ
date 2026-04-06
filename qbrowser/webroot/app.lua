local dark_mode = true
local text_color = dark_mode and
    { r = 255, g = 255, b = 255 } or
    { r = 0, g = 0, b = 0 }

local win_radius = 32
local mdown_old = false

local spacer = function(name)
  window.item {
    name = name,
    layout = {
      sizing = {
        width = window.sizing_grow(0),
        height = window.sizing_grow(0),
      },
    },
  }
end

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
    cornerRadius = { tl = win_radius },
    backgroundColor = { r = 42, g = 42, b = 42, a = 255 },
    drawchildren = function()
      window.item {
        name = "spacer_pre",
        layout = {
          sizing = {
            width = window.sizing_grow(0),
            height = window.sizing_grow(0),
          },
        },
        drawchildren = function()
          local header_hovered = window.is_hovered("header")
          window.drag = header_hovered and window.mdown
          window.item {
            name = "header",
            layout = {
              sizing = {
                width = window.sizing_grow(0),
                height = window.sizing_fixed(60),
              },
              padding = { t = 20, r = 20, },
            },
            cornerRadius = { tl = win_radius, tr = win_radius },
            drawchildren = function()
              spacer("spacer")

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
                    { r = 1, g = 0, b = 0 } or
                    { r = 0, g = 0, b = 0 },
                cornerRadius = { tl = 6 },
                image = exit_hovered and
                    "assets/xmark-circle-solid.png" or
                    "assets/xmark-circle.png"
              }
            end
          }
        end
      }

      window.item {
        name = "row",
        layout = {
          sizing = {
            width = window.sizing_grow(0),
          },
        },
        drawchildren = function()
          spacer("spacer_pre")

          window.item {
            name = "center",
            layout = {
              sizing = {
                width = window.sizing_fixed(500),
                height = window.sizing_fixed(60),
              },
              padding = { t = 14, b = 14, r = 14, l = 14 },
            },
            cornerRadius = { tl = win_radius },
            backgroundColor = { r = 58, g = 58, b = 58, a = 255 },
            drawchildren = function()
              window.text {
                text = "Enter Link: ",
                fontSize = 32,
                textColor = text_color,
              }
            end
          }

          spacer("spacer_post")
        end
      }

      spacer("spacer_post")
    end
  }
  mdown_old = window.mdown
end
