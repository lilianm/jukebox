#!/usr/bin/env ruby

require "playlist.rb"
require "display.rb"
require "id3.rb"

class Stream
  def play()
    sync();
  end

  def sid()
    @sid
  end

  def next()
    log("Next on channel #{@name}");
    @queue.next();
    fetchData();
  end

  def previous()
    log("Previous on channel #{@name}");
    @queue.previous();
    fetchData();
  end

  def getCurrentSongInfo()
    rsp           = @currentEntry.to_client();
    rsp[:elapsed] = @currentEntry.duration * @frame / @cur.size;
    rsp;
  end

  private
  
  def fetchData()
    begin
      nb_preload = 11
      
      delta     = [ nb_preload - @queue.size, 0 ].max;
      delta.times {
        # keep a file from being include twice in the next x songs
        last_insert = @queue[-nb_preload..-1] || [];
        begin
          entry = @library.get_file().first;
        end while last_insert.include?(entry.mid) # the space we look is (10 + preload) wide (30min) see above
        pos = @queue.add(entry.mid);
      }


      # move to the next entry
      mid = @queue[0];
      @currentEntry = @library.get_file(mid).first;
      file = @currentEntry.dst;
      log("Fetching on channel #{@name}: #{file}");
      data = File.open(file, &:read).force_encoding("BINARY");
      pos = 0;
      @cur = @currentEntry.frames.map { |b|
        f = data[pos, b];
        pos += b;
        f;
      }
      @frame = 0;
      tag = Id3.new();
      tag.title  = @currentEntry.title;
      tag.artist = @currentEntry.artist;
      tag.album  = @currentEntry.album;
      @tag = tag.to_s().force_encoding("BINARY");
      @timestamp = Time.now().to_i();
    rescue => e
      @queue.next() if(@queue[0]);
      error("Can't load mid=#{mid}: #{([ e.to_s ] + e.backtrace).join("\n")}", true, $error_file);
      retry;
    end
  end

  def sync()
    data = [];

    now = Time.now();
    if(@time == 0)
      delta = 0.2;
      @time = now - delta;
      fetchData()
    end

    if(@tag)
      data << @tag;
      @tag = nil;
    end

    delta = now - @time;
    @remaining += delta * @currentEntry.bitrate * 1000 / 8;
    begin
      @cur[@frame..-1].each { |f|
        data << f.force_encoding("BINARY");
        @remaining -= f.bytesize();
        @frame     += 1;
        break if(@remaining <= 0)
      }

      self.next() if(@remaining > 0);
    end while(@remaining > 0)
    @time  = now;
    data.join();
  end

  def generateIcyMetaData()
    str  = "";
    ch   = @data;
    meta = @currentEntry;

    if(meta && @meta != meta)
      str = "StreamTitle='#{meta.to_s().gsub("\'", " ")}';"
      @meta = meta;
    end
    
    padding = str.bytesize() % 16;
    padding = 16 - padding  if(padding != 0)
    str += "\x00" * padding;

    (str.bytesize()/16).chr + str;
  end

  @@last_sid = 0;

  def initialize(library, sid)
    @queue        = SongQueue.new();
    @library      = library;
    @sid          = sid;
    @time         = 0;
    @pos          = 0;
    @remaining    = 0;
    @icyRemaining = 4096
    @icyInterval  = 4096;
  end
end

class StreamPool < Rev::TimerWatcher
  def initialize(sck, library)
    super(0.2, true);
    @streams  = {};
    @sck      = sck;
    @library  = library;
    @next_sid = 1;
    attach(Rev::Loop.default) 
  end

  def create()
    sid = @next_sid;
    @next_sid += 1;
    s = Stream.new(@library, sid);
    @streams[sid] = s;
    log("Stream create sid=#{sid}");
    s;
  end

  def destroy(sid)
    s = @streams.delete(sid);
    log("Stream destroy sid=#{s.sid()}") if(s);
  end

  def get(sid)    
    @streams[sid];
  end

  private
  def on_timer
    @streams.each { |id, s|
      data = [ id ].pack("L") + s.play()
      while(data.bytesize != 0)
        data_send = data.slice!(0...32768);
        @sck.send_request(0x01, data_send);
      end
    }
  end
end
