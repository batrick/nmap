---
-- Pack and unpack binary data.
--
-- THIS LIBRARY IS DEPRECATED! Please use builtin Lua 5.3 string.pack facilities.
--
-- A problem script authors often face is the necessity of encoding values
-- into binary data. For example after analyzing a protocol the starting
-- point to write a script could be a hex dump, which serves as a preamble
-- to every sent packet. Although it is possible to work with the
-- functionality Lua provides, it's not very convenient. Therefore NSE includes
-- Binlib, based on lpack (http://www.tecgraf.puc-rio.br/~lhf/ftp/lua/)
-- by Luiz Henrique de Figueiredo.
--
-- The Binlib functions take a format string to encode and decode binary
-- data. Packing and unpacking are controlled by the following operator
-- characters:
-- * <code>H</code> hex string
-- * <code>B</code> bit string
-- * <code>x</code> null byte
-- * <code>z</code> zero-terminated string
-- * <code>p</code> string preceded by 1-byte integer length
-- * <code>P</code> string preceded by 2-byte integer length
-- * <code>a</code> string preceded by 4-byte integer length
-- * <code>A</code> string
-- * <code>f</code> float
-- * <code>d</code> double
-- * <code>n</code> Lua number
-- * <code>c</code> char (1-byte integer)
-- * <code>C</code> byte = unsigned char (1-byte unsigned integer)
-- * <code>s</code> short (2-byte integer)
-- * <code>S</code> unsigned short (2-byte unsigned integer)
-- * <code>i</code> int (4-byte integer)
-- * <code>I</code> unsigned int (4-byte unsigned integer)
-- * <code>l</code> long (8-byte integer)
-- * <code>L</code> unsigned long (8-byte unsigned integer)
-- * <code><</code> little endian modifier
-- * <code>></code> big endian modifier
-- * <code>=</code> native endian modifier
--
-- Note that the endian operators work as modifiers to all the
-- characters following them in the format string.

local _ENV = {}

local function translate (o, n)
    n = #n == 0 and 1 or tonumber(n)
    -- N.B. "A, B, or H, in which cases the number tells unpack how many bytes to read"
    if o == "H" then
        -- hex string
        -- FIXME we will need to change things so that 'H' becomes ('B'):rep(n) where n is #arg/2
    elseif o == "B" then
        -- bit string
        -- FIXME what is a "bit" string??
    elseif o == "p" then
        return ("s1"):rep(n)
    elseif o == "P" then
        return ("s2"):rep(n)
    elseif o == "a" then
        return ("s4"):rep(n)
    elseif o == "A" then
        -- an unterminated string
        -- FIXME we will need to change this to cn where n is the length of its argument
        return ("z"):rep(n)
    elseif o == "c" then
        return ("b"):rep(n)
    elseif o == "C" then
        return ("B"):rep(n)
    elseif o == "s" then
        return ("i2"):rep(n)
    elseif o == "S" then
        return ("I2"):rep(n)
    elseif o == "i" then
        return ("i4"):rep(n)
    elseif o == "I" then
        return ("I4"):rep(n)
    elseif o == "l" then
        return ("i8"):rep(n)
    elseif o == "L" then
        return ("I8"):rep(n)
    else
        return o:rep(n)
    end
end

--- Returns a binary packed string.
--
-- The format string describes how the parameters (<code>p1</code>,
-- <code>...</code>) will be interpreted. Numerical values following operators
-- stand for operator repetitions and need an according amount of parameters.
-- Operators expect appropriate parameter types.
--
-- Note: on Windows packing of 64-bit values > 2^63 currently
-- results in packing exactly 2^63.
-- @param format Format string, used to pack following arguments.
-- @param ... The values to pack.
-- @return String containing packed data.
function pack (format, ...)
    format = format:gsub("(%a)(%d*)", translate)
    return format:pack(...)
end

local function unpacker (...)
    -- Lua's unpack gives the stop index last:
    local list = table.pack(...)
    return list[list.n], table.unpack(list, 1, list.n-1)
end

--- Returns values read from the binary packed data string.
--
-- The first return value of this function is the position at which unpacking
-- stopped. This can be used as the <code>init</code> value for subsequent
-- calls. The following return values are the values according to the format
-- string. Numerical values in the format string are interpreted as repetitions
-- like in <code>pack</code>, except if used with <code>A</code>,
-- <code>B</code>, or <code>H</code>, in which cases the number tells
-- <code>unpack</code> how many bytes to read. <code>unpack</code> stops if
-- either the format string or the binary data string are exhausted.
-- @param format Format string, used to unpack values out of data string.
-- @param data String containing packed data.
-- @param init Optional starting position within the string.
-- @return Position in the data string where unpacking stopped.
-- @return All unpacked values.
function unpack (format, data, init)
    format = format:gsub("(%a)(%d*)", translate)
    return unpacker(fmt:unpack(s, init))
end

return _ENV
