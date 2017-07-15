#!/bin/bash
ngx_lua_commit=a5094766702d97ab43c742f26d6d55098aa1b1f0

download_github_repo_master() {
  local user=$1
  local repo=$2

  if [ ! -d $repo ]; then
    local commit=$(curl -sS https://api.github.com/repos/$user/$repo/commits/master | jq -r .sha)
    curl -sSL https://github.com/$user/$repo/archive/master.tar.gz | tar zxf -
    mv ${repo}-master $repo
    echo $user $repo $commit
  fi
}

if [ ! -d lua-nginx-module ]; then
  curl -sSL https://github.com/openresty/lua-nginx-module/archive/${ngx_lua_commit}.tar.gz | tar zxf -
  mv lua-nginx-module-${ngx_lua_commit} lua-nginx-module
fi

download_github_repo_master openresty headers-more-nginx-module
