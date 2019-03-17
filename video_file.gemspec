
lib = File.expand_path("../lib", __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require "video_file/version"

Gem::Specification.new do |spec|
  spec.name          = "video_file"
  spec.version       = VideoFile::VERSION
  spec.authors       = ["furunkel"]
  spec.email         = ["furunkel@polyadic.com"]
  spec.license       = "GPL"

  spec.summary       = %q{Get video metadata and create thumbnails}
  spec.homepage      = "https://www.github.com/furunkel/video_file"

  spec.extensions = Dir["ext/**/extconf.rb"]

  # Specify which files should be added to the gem when it is released.
  # The `git ls-files -z` loads the files in the RubyGem that have been added into git.
  spec.files         = Dir.chdir(File.expand_path('..', __FILE__)) do
    `git ls-files -z`.split("\x0").reject { |f| f.match(%r{^(test|spec|features)/}) }
  end
  spec.bindir        = "exe"
  spec.executables   = spec.files.grep(%r{^exe/}) { |f| File.basename(f) }
  spec.require_paths = ["lib"]

  spec.add_development_dependency "bundler", "~> 2.0"
  spec.add_development_dependency "rake", "~> 10.0"
  spec.add_development_dependency "minitest", "~> 5.0"
  spec.add_development_dependency "rake-compiler"
  spec.add_development_dependency "rmagick"
  spec.add_development_dependency "yard"
end
