#!/usr/bin/env ruby
$:.unshift File.dirname($0)

require 'ffi'

module Jukebox
  extend FFI::Library
  ffi_lib "./jukebox.so"

  attach_function :jukebox_init, [:int], :pointer
  attach_function :jukebox_launch, [], :void
end
