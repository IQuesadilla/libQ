local count = 0

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
    drawchildren = function()
      local header_hovered = window.is_hovered("header")
      window.drag = header_hovered and window.mdown
      window.item {
        name = "header",
        layout = {
          sizing = {
            width = window.sizing_grow(0),
          },
          padding = { t = 2, b = 2, r = 2, l = 2 },
        },
        backgroundColor = { r = 150, g = 150, b = 150 },
        drawchildren = function()
          local go_hovered = window.is_hovered("go")
          local go_pressed = go_hovered and not window.mdown and mdown_old
          if go_pressed then
            print("pressed")
          end
          window.drag = window.drag and not go_hovered

          window.item {
            name = "go",
            layout = {
              padding = { t = 3, b = 3, r = 6, l = 6 },
              childGap = 6,
            },
            backgroundColor = go_hovered and
                { r = 150, g = 90, b = 230 } or
                { r = 100, g = 100, b = 100 },
            cornerRadius = { tl = 6 },
            drawchildren = function()
              window.text("Go")
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
            image = exit_hovered and "assets/xmark-circle.png" or "assets/xmark-circle-solid.png"
          }
        end
      }

      window.item {
        name = "bodyOuter",
        layout = {
          sizing = {
            width = window.sizing_grow(0),
            height = window.sizing_grow(0),
          },
          padding = { t = 4, b = 4, r = 4, l = 4 },
        },
        backgroundColor = { r = 200, g = 200, b = 200, },
        drawchildren = function()
          window.item {
            name = "body",
            layout = {
              sizing = {
                width = window.sizing_grow(0),
                height = window.sizing_grow(0),
              },
              padding = { t = 4, b = 4, r = 4, l = 4 },
            },
            cornerRadius = { tl = 12 },
            backgroundColor = { r = 255, g = 255, b = 255 },
          }
        end
      }

      window.item {
        name = "footer",
        layout = {
          sizing = {
            width = window.sizing_grow(0),
          },
          padding = { t = 1, b = 1, r = 2, l = 2 },
        },
        drawchildren = function()
          window.text("Events: " .. tostring(count))
          count = count + 1
        end
      }
    end
  }
  mdown_old = window.mdown
end
