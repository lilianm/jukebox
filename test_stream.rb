#!/usr/bin/env ruby
$:.unshift File.dirname($0)

require "rev"
require "socket"

require "stream.rb"
require "db.rb"

require "jukebox_protocol.rb"

library = Library.new();

JukeboxProtocolListen.new(nil, 1234, library);
#JukeboxProtocolClient.new("maurel.biz", 1234);

Rev::Loop.default.run();

