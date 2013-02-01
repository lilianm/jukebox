#!/usr/bin/env ruby
$:.unshift File.dirname($0)

require 'ffi'

module Jukebox_wrapping
  extend FFI::Library
  ffi_lib "./jukebox.so"

  attach_function :jukebox_init, [:int], :pointer
  attach_function :jukebox_launch, [], :void

  callback :request_cb, [:pointer, :pointer, :string, :int], :int

  attach_function :http_send_reponse_and_dup, [:pointer, :string, :string, :int], :void
  attach_function :http_node_new, [:pointer, :string, :request_cb, :pointer], :void

  attach_function :http_request_get_data, [:pointer], :pointer

  attach_function :user_get_channel, [:pointer], :pointer

  attach_function :channel_next, [:pointer], :int
end


module Jukebox
  def init(port)
    Jukebox_wrapping::jukebox_init(port)
  end

  def launch()
    Jukebox_wrapping::jukebox_launch();
  end

  class Channel
    def initialize(user_hdl)
      @hdl = Jukebox_wrapping::user_get_channel(user_hdl);
    end

    def next()
      Jukebox_wrapping::channel_next(@hdl);
    end
  end

  module Http
    def send_reponse(hdl, content_type = nil, reponse)
      content_type ||= "text/html"

      Jukebox_wrapping::http_send_reponse_and_dup(hdl, content_type, reponse, reponse.size);
    end

    def node_new(hdl, path,  &block)
      callback = Proc.new { |hdl,data,remaining,size|
        user_hdl = Jukebox_wrapping::http_request_get_data(hdl);
        channel  = Channel.new(user_hdl);
        rem      = remaining[0...size];
        block.call(hdl, data, rem, channel);
      }
      Jukebox_wrapping::http_node_new(hdl, path, callback, nil);
    end
  end
end
