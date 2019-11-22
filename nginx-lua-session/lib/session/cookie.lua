-- Copyright (C) by Hiroaki Nakamura (hnakamur)

local resty_cookie = require "resty.cookie"
local setmetatable = setmetatable

local _M = { _VERSION = '0.1.0' }

local mt = { __index = _M }

function _M.new(self, config)
    local cookie_manager, err = resty_cookie:new()
    if not cookie_manager then
        return nil,
            string.format("error to create resty.cookie instance for session cookie, err=%s", err)
    end

    return setmetatable({
        cookie_manager = cookie_manager,
        name = config.name,
        secure = config.secure,
        path = config.path
    }, mt)
end

function _M.get(self)
    local value, err = self.cookie_manager:get(self.name)
    if err == "no cookie found in the current request" then
        return nil, nil
    end
    if err ~= nil then
        return nil, string.format("error to get session cookie, key=%s", self.name)
    end
    return value
end

function _M.set(self, session_id)
    local ok, err = self.cookie_manager:set{
        key = self.name, value = session_id,
        path = self.path, secure = self.secure, httponly = true
    }
    if not ok then
        return false,
            string.format("error to set session cookie, key=%s, value=%s, path=%s, secure=%s, err=%s",
                          self.name, session_id, self.path, self.secure, err)
    end
    return true
end

function _M.delete(self)
    local ok, err = self.cookie_manager:set{
        key = self.name, value = "",
        path = self.path, secure = self.secure, httponly = true,
        expires = "Thu Jan 01 1970 00:00:00 GMT"
    }
    if not ok then
        return false,
            string.format("error to delete session cookie, key=%s, path=%s, secure=%s, err=%s",
                          self.name, session_id, self.path, self.secure, err)
    end
    return true
end

return _M
