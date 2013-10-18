#!/usr/bin/env ruby

require "rev"
require "socket"

REPONSE_MASK = 0x80000000

class JukeboxProtocol < Rev::Socket
  def send_request(id = 0, type, data)
    hdr = [ id, type, data.bytesize ].pack("LSS");
    puts "send #{data.bytesize}"

    write(hdr + data);
  end

  def on_read(data)
    data.force_encoding("BINARY");
    @data += data;
    if(@size == nil)
      return if(@data.bytesize < 8);
      @id, @type, @size = @data.unpack("LSS");
      @data = @data[8..-1];
    end
    return if(@data.bytesize() < @size);
    data  = @data[0...@size]
    @data = @data[@size..-1]
    size  = @size
    @size = nil;

    puts "-> req id #{@id} type #{@type} size #{size} #{data.bytesize}"

    on_request(@id, @type, data)
  end

  def initialize(sck)
    super(sck);
    @data = "".force_encoding("BINARY");
  end

end

class JukeboxProtocolServer < JukeboxProtocol
  def initialize(sck, library)
    super(sck);

    @pool = StreamPool.new(self, library);
    puts "init"
  end  

  def on_close()
    @pool.detach()
    super
  end

  def on_request(id, type, data)
    case(type)
    when 0x00
      s = @pool.create()

      data = [ s.sid ].pack("l")
      send_request(0 | REPONSE_MASK, data);
    when 0x02
      sid = data.unpack("L");
      s = @pool.get(sid.first);
      s.next() if(s);
    when 0x03
      sid = *data.unpack("L");
      s = @pool.get(sid.first);
      s.previous()
    end
  end
end

class JukeboxProtocolListen < Rev::TCPServer
  def initialize(addr, port, library)
    super(addr, port, JukeboxProtocolServer, library);
    attach(Rev::Loop.default)
  end
end

class JukeboxProtocolClient < JukeboxProtocol
  def initialize(addr, port)
    s = TCPSocket.open(addr, port);
    super(s)
  end
end
