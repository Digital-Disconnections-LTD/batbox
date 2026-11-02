class Batbox < Formula
  desc "Terminal coding agent for Anthropic Messages API servers"
  homepage "https://github.com/Digital-Disconnections-LTD/batbox"
  url "https://github.com/Digital-Disconnections-LTD/batbox/archive/refs/tags/v0.1.0.tar.gz"
  # PLACEHOLDER: replace with the sha256 of the v0.1.0 tarball once the tag exists (see RELEASING.md).
  sha256 "PLACEHOLDER"
  license "MIT"
  head "https://github.com/Digital-Disconnections-LTD/batbox.git", branch: "main"

  depends_on "cmake" => :build
  depends_on "libwebsockets" => :build
  depends_on "ninja" => :build
  depends_on "nlohmann-json" => :build
  depends_on "pkgconf" => :build
  depends_on "cmark-gfm"
  depends_on "curl"
  depends_on "glib"
  depends_on "icu4c@79"
  depends_on "libuv"
  depends_on "openssl@3"
  depends_on "pcre2"
  depends_on "ripgrep"
  depends_on "vips"
  depends_on "yaml-cpp"

  on_macos do
    depends_on "gettext"
  end

  resource "ftxui" do
    url "https://github.com/ArthurSonzogni/FTXUI/archive/f921fad208912747c17d129a8ef75ec7624b6eec.tar.gz"
    sha256 "00e1426f5b13d7748af44925768cca3bc4f64aa3b02e4c82567fd85716911a5b"
  end

  resource "ada" do
    url "https://github.com/ada-url/ada/archive/refs/tags/v4.0.0.tar.gz"
    sha256 "6d6c7ef7dd2e329320d34eb2ab29ccdc879ee3935af9dfb894a6640e58dc381d"
  end

  def install
    resource("ftxui").stage buildpath/"deps/ftxui"
    resource("ada").stage buildpath/"deps/ada"

    args = %W[
      -DBUILD_TESTING=OFF
      -DBATBOX_FETCH_CMARK_GFM=OFF
      -DFETCHCONTENT_FULLY_DISCONNECTED=ON
      -DFETCHCONTENT_SOURCE_DIR_BATBOX_FTXUI_UPSTREAM=#{buildpath}/deps/ftxui
      -DBATBOX_ADA_SOURCE_DIR=#{buildpath}/deps/ada
    ]
    system "cmake", "-S", ".", "-B", "build", "-G", "Ninja", *args, *std_cmake_args
    system "cmake", "--build", "build", "--target", "batbox"
    system "cmake", "--install", "build"
  end

  test do
    assert_match(/^batbox \d+\.\d+\.\d+$/, shell_output("#{bin}/batbox --version"))
    assert_match "Usage: batbox", shell_output("#{bin}/batbox --help")
  end
end
