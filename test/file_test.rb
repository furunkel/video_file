require "test_helper"

module VideoFile
  class FileTest < Minitest::Test
    def test_non_existend_file
      error = assert_raises VideoFile::Error do
        VideoFile::File.new ::File.join(__dir__, 'life-is-beautiful.webm')
      end
      assert_match(/no such file/i, error.message)
    end

    def test_metadata
      file = VideoFile::File.new ::File.join(__dir__, 'big-buck-bunny_trailer.webm')
      assert_equal 32, file.duration.round
      assert_equal 640, file.width
      assert_equal 360, file.height
      assert_in_delta 1.77, file.aspect_ratio, 0.01
      assert_in_delta 25, file.fps, 0.01
    end
  end
end
