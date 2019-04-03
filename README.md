[![Yard Docs](http://img.shields.io/badge/yard-docs-blue.svg)](http://rubydoc.info/github/furunkel/video_file/master/frames)

# VideoFile

A simple gem for generating video thumbnails.

## Installation

Install FFMPEG and TurboJPEG. For Ubuntu/Debian:
```
$ sudo apt install libavcodec-dev libavformat-dev libturbojpeg0-dev libswscale-dev
```

Add this line to your application's Gemfile:

```ruby
gem 'video_file'
```

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install video_file

## Usage

```ruby
file = VideoFile::File.new 'video.mp4'
puts file.duration

thumbnailer = VideoFile::Thumbnailer.new file, 128
jpeg_data = thumbnailer.get 3.0
File.write('thumbnail.jpg', jpeg_data)
```


## Development

After checking out the repo, run `bin/setup` to install dependencies. Then, run `rake test` to run the tests. You can also run `bin/console` for an interactive prompt that will allow you to experiment.

To install this gem onto your local machine, run `bundle exec rake install`. To release a new version, update the version number in `version.rb`, and then run `bundle exec rake release`, which will create a git tag for the version, push git commits and tags, and push the `.gem` file to [rubygems.org](https://rubygems.org).

## Contributing

Bug reports and pull requests are welcome on GitHub at https://github.com/furunkel/video_file.
