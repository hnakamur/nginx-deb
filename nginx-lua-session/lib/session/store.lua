-- Copyright (C) by Hiroaki Nakamura (hnakamur)

require "resty.core"
local setmetatable = setmetatable

local _M = { _VERSION = '0.1.0' }

local mt = { __index = _M }

function _M.new(self, config)
    local dict_name = config.dict_name
    local dict = ngx.shared[dict_name]
    return setmetatable({
        dict_name = dict_name,
        dict = dict,
        id_generator = config.id_generator,
        exptime = config.exptime or 0
    }, mt)
end

function _M.add(self, data)
    local dict = self.dict
    local id_generator = self.id_generator
    local exptime = self.exptime

    local session_id, success, err, forcible
    repeat
        session_id = id_generator()
        success, err, forcible = dict:add(session_id, data, exptime)
    until success or err ~= "exists"

    if not success then
        return nil,
            string.format("error to add session, dict=%s, session_id=%s, err=%s, forcible=%s",
                          self.dict_name, session_id, err, forcible)
    end

    return session_id
end

function _M.extend(self, session_id)
    local dict = self.dict
    local success, err = dict:expire(session_id, self.exptime)
    if success or err == "not found" then
        return success, err
    end

    return false,
        string.format("error to extend session, dict=%s, session_id=%s, err=%s",
                      self.dict_name, session_id, err)
end

function _M.get(self, session_id)
    return self.dict:get(session_id)
end

function _M.delete(self, session_id)
    self.dict:delete(session_id)
end

return _M
