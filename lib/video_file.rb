require 'video_file/version'
require 'video_file_ext'

module VideoFile
  class Thumbnailer
    # Obtain a thumbnail at the given position
    #
    # @param position [Numeric] position in seconds
    # @param accurate [Boolean] whether accurate seeking is desired
    # @param filter [Boolean] whether to filter out black, white and monton frames
    # @return [String] JPEG-encoded thumbnail
    def get(position, accurate: false, filter: false)
      get__(position, accurate, filter)
    end
  end
end
