#!/usr/bin/env ruby
$:.unshift File.dirname($0)

require 'jukebox'
include Jukebox
include Jukebox::Http

http_server = Jukebox::init(8085);

Jukebox::Http::node_new(http_server, "/api/basic/next") { |hdl,data,remaining, channel|
  channel.next();
  page =  "<head><title>Next</title></head><body><h1>Next</h1></body>"
  Jukebox::Http::send_reponse(hdl, "text/html", page)
  0
}

Jukebox::launch();
