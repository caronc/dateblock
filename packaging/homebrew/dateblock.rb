# Homebrew formula for dateblock (macOS and Linux).
#
# @VERSION@ and @SHA256@ are filled in by 'make release-recipes', which
# writes the finished formula to dist/recipes/homebrew/dateblock.rb.
class Dateblock < Formula
  desc "Block (sleep) until a cron-style moment in time"
  homepage "https://github.com/caronc/dateblock"
  url "https://github.com/caronc/dateblock/archive/refs/tags/v@VERSION@.tar.gz"
  sha256 "@SHA256@"
  license "GPL-2.0-or-later"
  head "https://github.com/caronc/dateblock.git", branch: "master"

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "libtool" => :build

  def install
    system "./autogen.sh"
    system "./configure", "--disable-static", *std_configure_args
    system "make", "install"
  end

  test do
    # --test shows when it would unblock, without waiting
    assert_match "Block Until", shell_output("#{bin}/dateblock -t -n 5")

    # A command after the options runs once the time is reached, and its
    # exit code becomes dateblock's
    shell_output("#{bin}/dateblock -s /1 -- sh -c 'exit 7'", 7)
  end
end
