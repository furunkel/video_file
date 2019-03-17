require "test_helper"

class VideoFileTest < Minitest::Test
  def test_that_it_has_a_version_number
    refute_nil ::VideoFile::VERSION
  end
end
