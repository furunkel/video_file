require "test_helper"

module VideoFile
  class ThumbnailerTest < Minitest::Test
    def test_nil_file
      error = assert_raises TypeError do
        thumbnailer = VideoFile::Thumbnailer.new nil, 10, 10
      end
    end

    def test_get
      require 'rmagick'
  
      count = 4
      width = 256
      file = VideoFile::File.new ::File.join(__dir__, 'big-buck-bunny_trailer.webm')
      thumbnailer = VideoFile::Thumbnailer.new file, width, count

      jpeg1 = thumbnailer.get 4, accurate: true
      image = Magick::Image.from_blob(jpeg1).first
      assert_equal width * count, image.columns

      jpeg2 = thumbnailer.get 4, accurate: true
      assert_equal jpeg1, jpeg2
    end
  end
end
