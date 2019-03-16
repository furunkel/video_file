require 'video_file/version'
require 'video_file_ext'

module VideoFile
  class Thumbnailer
    def get(position, accurate: false, filter_monoton: false)
      get__(position, accurate, filter_monoton)
    end
  end
end
