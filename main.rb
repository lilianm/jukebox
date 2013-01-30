#!/usr/bin/env ruby
$:.unshift File.dirname($0)

require 'jukebox'

Jukebox::jukebox_init(8085);
Jukebox::jukebox_launch();
