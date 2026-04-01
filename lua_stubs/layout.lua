---@meta
---
--- The rendering interface here maps directly to clay:
--- https://github.com/nicbarker/clay

---@class Color
---@field r nil|integer
---@field g nil|integer
---@field b nil|integer
---@field a nil|integer

---@class SizingAxis
---@field type integer
---@field percent number
---@field min number
---@field max number

---@class Sizing
---@field width nil|SizingAxis
---@field height nil|SizingAxis

---@class Layout
---@field sizing nil|Sizing

---@class DrawCommand
---@field name string
---@field layout nil|Layout
---@field backgroundColor nil|Color
---@field handler nil|fun(id:string):nil

---@class window
window = {}

---@param fixedSize number
---@return SizingAxis
function window.sizing_fixed(fixedSize) end

---@param min number
---@param max number
---@return SizingAxis
function window.sizing_grow(min, max) end

---@param percent number
---@return SizingAxis
function window.sizing_percent(percent) end

---@param min number
---@param max number
---@return SizingAxis
function window.sizing_fit(min, max) end

---@param lay DrawCommand
function window.draw(lay) end

---@param text string
function window.text(text) end
