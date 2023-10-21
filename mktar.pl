#! /usr/bin/perl -w
# 
# create a tar ball snapshot from cvs repository.
#
# 2005-12-15, jw
#

use Data::Dumper;

my $srcdir = 'src';
my $version_file = "$srcdir/configure.ac";
my %symlinks = 
(
  'FAQ' => 'doc/FAQ',
  'doc/install.sh' => '../install.sh'
);

for my $l (keys %symlinks)
  {
    my $ll = "$srcdir/$l";
    next if -l $ll;
    print "replacing $ll with a symlink to $symlinks{$l}\n";
    unlink $ll;
    symlink $symlinks{$l}, $ll;
  }

my %version;
open IN, "<", $version_file or die "unable to read $version_file: $!";
$version = $1 if <IN> =~ /AC_INIT\(\[[^\]]+\],\[(\d+\.\d+\.\d+)\]/;
close IN;

my $tmpdir = "/tmp/mktar-$^T";
mkdir $tmpdir or die "cannot mkdir $tmpdir: $!";
system "cp -a $srcdir $tmpdir" and die "'cp -a $srcdir $tmpdir' failed: $!";
rename "$tmpdir/$srcdir", "$tmpdir/screen-$version" or die "rename to screen-$version failed: $!";

system "tar zcf - -C $tmpdir > screen-$version.tar.gz screen-$version";
system "rm -rf $tmpdir";

print "screen-$version.tar.gz written.\n";
