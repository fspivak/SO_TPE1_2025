alias ls="ls --color"
alias dcrun='docker run -v "${PWD}:/root" --privileged -ti --name SO-TPE1 agodio/itba-so-multi-platform:3.0'
alias dcexec='docker exec -ti SO-TPE1 bash'
alias dcstart='docker start SO-TPE1'
alias dcstop='docker stop SO-TPE1'
alias dcrm='docker rm -f SO-TPE1'
export GCC_COLORS='error=01;31:warning=01;35:note=01;36:caret=01;32:locus=01:quote=01'