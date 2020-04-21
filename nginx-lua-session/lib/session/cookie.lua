-- Copyright (C) by Hiroaki Nakamura (hnakamur)

local resty_cookie = require "resty.cookie"
local setmetatable = setmetatable

local _M = { _VERSION = '0.2.0' }

local mt = { __index = _M }

local function _update(dt, ...)
    for i = 1, select('#', ...) do
        local t = select(i, ...)
        if t then
            for k,v in pairs(t) do
                dt[k] = v
            end
        end
    end
    return dt
end

function _M.new(self, config)
    local cookie_manager, err = resty_cookie:new()
    if not cookie_manager then
        return nil,
            string.format("error to create resty.cookie instance for session cookie, err=%s", err)
    end

    local obj = _update({
        cookie_manager = cookie_manager,
        secure = true,
        httponly = true,
    }, config)
    return setmetatable(obj, mt)
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

function _M.set(self, value, opts)
    local cookie_opts = _update({
        key = self.name, value = value,
        path = self.path, domain = self.domain,
        secure = self.secure, httponly = self.httponly,
        expires = self.expires, max_age = self.max_age,
        samesite = self.samesite, extension = self.extension,
    }, opts)
    local ok, err = self.cookie_manager:set(cookie_opts)
    if not ok then
        return false,
            string.format("error to set session cookie, key=%s, value=%s, err=%s",
                          self.name, value, err)
    end
    return true
end

function _M.delete(self, opts)
    local cookie_opts = _update({
        key = self.name, value = '',
        path = self.path, domain = self.domain,
        secure = self.secure, httponly = self.httponly,
        expires = 'Thu Jan 01 1970 00:00:00 GMT', max_age = self.max_age,
        samesite = self.samesite, extension = self.extension,
    }, opts)
    local ok, err = self.cookie_manager:set(cookie_opts)
    if not ok then
        return false,
            string.format("error to delete session cookie, key=%s, path=%s, secure=%s, err=%s",
                          self.name, session_id, self.path, self.secure, err)
    end
    return true
end

return _M
