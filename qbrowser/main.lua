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
    backgroundColor = { r = 30, g = 30, b = 30 },
    drawchildren = function()
      window.item {
        name = "header",
        layout = {
          sizing = {
            width = window.sizing_grow(0),
          },
          padding = { t = 2, b = 2, r = 2, l = 2 },
        },
        backgroundColor = { r = 90, g = 90, b = 90 },
        drawchildren = function()
          local hovered = window.is_hovered("go")
          if hovered and not window.mdown and mdown_old then
            print("pressed")
          end
          window.item {
            name = "go",
            layout = {
              padding = { t = 3, b = 3, r = 6, l = 6 },
            },
            backgroundColor = hovered and
                { r = 150, g = 90, b = 230 } or
                { r = 100, g = 100, b = 100 },
            cornerRadius = { tl = 6 },
            drawchildren = function()
              window.text("Go")
            end
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
        },
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
