#!/bin/bash
ngx_lua_commit=a5094766702d97ab43c742f26d6d55098aa1b1f0

download_github_repo() {
  local user=$1
  local repo=$2
  local commit=$3

  if [ ! -d $repo ]; then
    if [ $commit = master ]; then
      commit=$(curl -sS https://api.github.com/repos/$user/$repo/commits/master | jq -r .sha)
    fi
    curl -sSL https://github.com/$user/$repo/archive/${commit}.tar.gz | tar zxf -
    mv ${repo}-${commit} $repo
    echo $user/$repo $commit
  fi
}

download_github_repo openresty lua-nginx-module a5094766702d97ab43c742f26d6d55098aa1b1f0
download_github_repo openresty headers-more-nginx-module master
download_github_repo openresty replay ngx_http_secure_download master
download_github_repo openresty replay ngx_http_consistent_hash master
