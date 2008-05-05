#!/bin/sh

(cd .. ; grep -l 'N\?_[[:space:]]*(.*)' ./src/*.c) > POTFILES.in
