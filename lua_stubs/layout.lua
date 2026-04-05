---@meta
---
--- The rendering interface here maps directly to clay:
--- https://github.com/nicbarker/clay

---@class Color
---@field r number
---@field g number
---@field b number
---@field a nil|number

---@class SizingAxis
---@field type integer
---@field percent number
---@field min number
---@field max number

---@class Sizing
---@field width nil|SizingAxis
---@field height nil|SizingAxis

---@class Padding
---@field t nil|integer
---@field r nil|integer
---@field b nil|integer
---@field l nil|integer

---@class CornerRadius
---@field tl nil|number
---@field tr nil|number
---@field bl nil|number
---@field br nil|number

---@class Layout
---@field sizing nil|Sizing
---@field layoutDirection nil|integer
---@field padding nil|Padding
---@field childGap nil|integer

---@class Item
---@field name string
---@field layout nil|Layout
---@field backgroundColor nil|Color
---@field cornerRadius nil|CornerRadius
---@field image nil|string
---@field drawchildren nil|fun(id:string):nil

---@class Text
---@field text string
---@field fontSize integer
---@field textColor Color

---@class window
window = {}

window.drag = false
window.mdown = false

---@param fixedSize number
---@return SizingAxis
function window.sizing_fixed(fixedSize) end

---@param min number
---@return SizingAxis
function window.sizing_grow(min) end

---@param percent number
---@return SizingAxis
function window.sizing_percent(percent) end

---@param min number
---@return SizingAxis
function window.sizing_fit(min) end

---@param name string
---@return boolean
function window.is_hovered(name) end

---@param lay Item
function window.item(lay) end

---@param text Text
function window.text(text) end

---@param rc integer
function window.close(rc) end
