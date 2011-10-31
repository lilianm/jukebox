#!/usr/bin/env ruby

require 'rev'
require 'thread'
load 'id3.rb'

ENCODE_DELAY_SCAN = 30; # seconds
MAX_ENCODE_JOB    = 2;

class EncodingThread < Rev::IO
  def initialize(file, *args, &block)
    @block = block;
    @args  = args;

    begin 
      mid, src, dst, title, artist, album, years, status = file;
      log("Encoding #{src} -> #{dst}");

      src = src.sub('"', '\"');
      dst = dst.sub('"', '\"');

      rd, wr = IO.pipe
      @pid = fork {
        rd.close()
        STDOUT.reopen(wr)
        wr.close();
        exec("mpg123 --stereo -r 44100 -s \"#{src}\" | lame - \"#{dst}\" -r -b 192 -t > /dev/null 2> /dev/null");
      }
      debug("Process encoding PID=#{@pid}");
      wr.close();
      @fd  = rd;
      super(@fd);
    rescue => e
      error("Encode execution error on file #{src}: #{e.to_s}", true, $error_file);
      @block.call(255, *@args);
    end
  end


  private
  def on_read(data)
    puts data;
  end

  def on_close()
    pid, status = Process.waitpid2(@pid);
    @block.call(status.exitstatus(), *@args);
  end
end

class Encode < Rev::TimerWatcher
  def initialize(library, originDir, encodedDir)
    @originDir            = originDir;
    @encodedDir           = encodedDir;
    @library              = library;
    @th                   = [];
    super(ENCODE_DELAY_SCAN, true);
    scan();
  end

  def files()
    @files;
  end

  def nextEncode(th)
    @th.delete(th);
    encode();
  end

  def attach(loop)
    @loop = loop;
    @th.each { |t|
      t.attach(@loop);
    }
    super(loop);
  end

  private

  def encode()
    return if(@th.size >= MAX_ENCODE_JOB);

    file = @library.encode_file()
    return if(file == nil);

    mid = file[0];

    @library.change_stat(mid, Library::FILE_ENCODING_PROGRESS);
    enc = EncodingThread.new(file, self, @library, mid) { |status, obj, lib, mid|
      if(status == 0)
        @library.change_stat(mid, Library::FILE_OK)
      else
        @library.change_stat(mid, Library::FILE_ENCODING_FAIL)
      end
      obj.nextEncode(enc);
    }
    enc.attach(@loop) if(@loop != nil);
    @th.push(enc);

    encode();
  end

  def on_timer
    scan();
  end

  def scan()
    files  = Dir.glob(@originDir + "/*.mp3");
    signal = false;

    files.each { | f |
      name = f.scan(/.*\/(.*)/);
      name = name[0][0];
      if(@library.check_file(f))
        tag = Id3.decode(f);
        if(tag.title == nil || tag.artist == nil || tag.album == nil)
          @library.add(f, @encodedDir,
                       tag.title, tag.artist, tag.album, tag.date,
                       Library::FILE_BAD_TAG);
        else
          @library.add(f, @encodedDir + "/" + name,
                       tag.title, tag.artist, tag.album,
                       tag.date, Library::FILE_WAIT);
        end
      end
    }

    encode();
  end

end

