#!/usr/bin/env ruby
$:.unshift File.dirname($0)

require 'jukebox'
require 'json'

include Jukebox
include Jukebox::Http

http_server = Jukebox::init(8085);

Jukebox::Http::node_new(http_server, "/api/basic/next") { |hdl,data,remaining, channel|
  channel.next();
  page =  "<head><title>Next</title></head><body><h1>Next</h1></body>"
  Jukebox::Http::send_reponse(hdl, "text/html", page)
  0
}

Jukebox::Http::node_new(http_server, "/api/basic/previous") { |hdl,data,remaining, channel|
  channel.previous();
  page =  "<head><title>Previous</title></head><body><h1>Previous</h1></body>"
  Jukebox::Http::send_reponse(hdl, "text/html", page)
  0
}

Jukebox::Http::node_new(http_server, "/api/json") { |hdl,data,remaining,channel, content|
  resp = { :timestamp => Time.now.to_i() };

  def parse_action(resp, channel, req)
    return if(channel == nil)
    def forward_action(resp, req, ch)
      case(req["name"])
      when "next"
        ch.next();
      when "previous"
        ch.previous();
      end
    end

    if( req.kind_of?(Array) )
      req.each { |currentAction| 
        # Warning multi action should merge responses
        forward_action(resp, currentAction, channel);
      }
    else
      forward_action(resp, req, channel);
    end
  end

  begin
    json = JSON.parse(content);
    timestamp = json.delete("timestamp") || 0;
    json.each { |type, value|
      case(type)
#      when "search"
#        parse_search(resp, value);
      when "action"
        parse_action(resp, channel, value);
      else
#        JsonManager.add_message(resp, MSG_LVL_ERROR, "unknown command #{type}", "Unknown command #{type}");
      end
    }
    # refresh

    s = channel.current;

    resp[:channel_infos] = { :elapsed => 0, :listener_count => 1 }
    resp[:current_song]  = { 
      :elapsed  => channel.elapsed() / 1000000, 
      :mid      => s[:mid],
      :title    => s[:title].force_encoding("UTF-8"),
      :artist   => s[:artist].force_encoding("UTF-8"),
      :album    => s[:album].force_encoding("UTF-8"),
      :years    => s[:years],
      :track    => s[:track],
      :trackNb  => s[:track_nb],
      :status   => s[:status],
      :mtime    => s[:mtime],
      :bitrate  => s[:bitrate],
      :duration => s[:duration] / 1000000
    }
  end
  str = JSON.generate(resp)

  Jukebox::Http::send_reponse(hdl, "application/json", str);
  0
}

Jukebox::launch();
