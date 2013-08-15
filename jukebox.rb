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

  attach_function :http_request_get_content, [:pointer, :pointer], :string

  attach_function :http_request_get_data, [:pointer], :pointer

  attach_function :user_get_channel, [:pointer], :pointer

  attach_function :channel_next, [:pointer], :int
  attach_function :channel_previous, [:pointer], :int
  attach_function :channel_current_song, [:pointer], :pointer
  attach_function :channel_current_song_elapsed, [:pointer], :int
  attach_function :channel_get_queue, [:pointer], :pointer

  attach_function :song_queue_get, [:pointer, :int], :int
  attach_function :song_get, [:int], :pointer
end


module Jukebox
  def init(port)
    Jukebox_wrapping::jukebox_init(port)
  end

  def launch()
    Jukebox_wrapping::jukebox_launch();
  end

  class Song < FFI::Struct
    layout :mid,      :int,
           :src,      :string,
           :dst,      :string,
           :title,    :string,
           :artist,   :string,
           :album,    :string,
           :years,    :int,
           :track,    :int,
           :track_nb, :int,
           :genre,    :int,
           :status,   :int,
           :bitrate,  :int,
           :duration, :int,
           :mtime,    :int;
  end

  class Channel
    def initialize(user_hdl)
      p user_hdl
      p user_hdl.null?
      raise "User handle not valid" if(user_hdl == nil || user_hdl.null?);
      @hdl = Jukebox_wrapping::user_get_channel(user_hdl);
    end

    def next()
      Jukebox_wrapping::channel_next(@hdl);
    end

    def previous()
      Jukebox_wrapping::channel_previous(@hdl);
    end

    def current()
      s = Jukebox_wrapping::channel_current_song(@hdl);
      return nil if(s == nil);

      queue()
      Song.new(s);
    end

    def elapsed()
      Jukebox_wrapping::channel_current_song_elapsed(@hdl)
    end

    def queue()
      q   = Jukebox_wrapping::channel_get_queue(@hdl)
      i   = 0;
      res = [];
      while((mid = Jukebox_wrapping::song_queue_get(q, i)) != -1)
        s = Song.new(Jukebox_wrapping::song_get(mid));
        res.push({ :mid      => s[:mid],
                   :title    => s[:title].force_encoding("UTF-8"),
                   :artist   => s[:artist].force_encoding("UTF-8"),
                   :album    => s[:album].force_encoding("UTF-8"),
                   :duration => s[:duration] / 1000000});
        i = i + 1;
      end
      p res
      res
    end
  end

  module Http
    def send_reponse(hdl, content_type = nil, reponse)
      content_type ||= "text/html"

      Jukebox_wrapping::http_send_reponse_and_dup(hdl, content_type, reponse, reponse.size);
    end

    def node_new(hdl, path,  &block)
      callback = Proc.new { |hdl,data,remaining,size|
        user_hdl     = Jukebox_wrapping::http_request_get_data(hdl);
        channel      = Channel.new(user_hdl);
        rem          = remaining[0...size];
        content_size = FFI::MemoryPointer.new(:size_t, 1)
        content      = Jukebox_wrapping::http_request_get_content(hdl, content_size);
        content_size = content_size.read_int();
        content      = content[0...content_size] if(content)

        block.call(hdl, data, rem, channel, content);
      }
      Jukebox_wrapping::http_node_new(hdl, path, callback, nil);
    end
  end
end
