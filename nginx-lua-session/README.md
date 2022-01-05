nginx-lua-session
=================

A simple session library using [openresty/lua-nginx-module: Embed the Power of Lua into NGINX HTTP servers](https://github.com/openresty/lua-nginx-module).

It generates a 128bit random session key and store session data in a nginx shared dict with that key.
It also send the session id with a cookie.

It is meant to be used by [hnakamur/nginx-lua-saml-service-provider](https://github.com/hnakamur/nginx-lua-saml-service-provider).

For a session library with more features, [bungle/lua-resty-session: Session library for OpenResty – flexible and secure](https://github.com/bungle/lua-resty-session) looks promising.


## Dependencies

* [cloudflare/lua-resty-cookie: Lua library for HTTP cookie manipulations for OpenResty/ngx_lua](https://github.com/cloudflare/lua-resty-cookie)
* [openresty/lua-resty-core: New FFI-based API for lua-nginx-module](https://github.com/openresty/lua-resty-core)
