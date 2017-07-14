#!/bin/bash
ngx_lua_commit=a5094766702d97ab43c742f26d6d55098aa1b1f0

if [ ! -d lua-nginx-module ]; then
  curl -sSL https://github.com/openresty/lua-nginx-module/archive/${ngx_lua_commit}.tar.gz | tar zxf -
  mv lua-nginx-module-${ngx_lua_commit} lua-nginx-module
fi
