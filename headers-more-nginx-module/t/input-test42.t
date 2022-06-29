# vi:filetype=

use lib 'lib';
use Test::Nginx::Socket; # 'no_plan';

repeat_each(2);

plan tests => repeat_each() * 3;

no_long_string();
#no_diff;

run_tests();

__DATA__

=== TEST 42: clear all and re-insert
--- main_config
    load_module /etc/nginx/modules/ngx_http_echo_module.so;
    load_module /etc/nginx/modules/ngx_http_headers_more_filter_module.so;
--- config
    location = /t {
        more_clear_input_headers Host Connection Cache-Control Accept
                                 User-Agent Accept-Encoding Accept-Language
                                 Cookie;

        more_set_input_headers "Host: a" "Connection: b" "Cache-Control: c"
                               "Accept: d" "User-Agent: e" "Accept-Encoding: f"
                               "Accept-Language: g" "Cookie: h";

        more_clear_input_headers Host Connection Cache-Control Accept
                                 User-Agent Accept-Encoding Accept-Language
                                 Cookie;

        more_set_input_headers "Host: a" "Connection: b" "Cache-Control: c"
                               "Accept: d" "User-Agent: e" "Accept-Encoding: f"
                               "Accept-Language: g" "Cookie: h";

        echo ok;
    }

--- raw_request eval
"GET /t HTTP/1.1\r
Host: localhost\r
Connection: close\r
Cache-Control: max-age=0\r
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r
User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_9_0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/30.0.1599.101 Safari/537.36\r
Accept-Encoding: gzip,deflate,sdch\r
Accept-Language: en-US,en;q=0.8\r
Cookie: test=cookie;\r
\r
"
--- response_body
ok
--- no_error_log
[error]
