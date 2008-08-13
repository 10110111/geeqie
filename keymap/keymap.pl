#!/usr/bin/perl
use strict;

my %funcs;

open(ACCELS, "<$ENV{HOME}/.geeqie/accels") or die "No accel file";
while (<ACCELS>)
	{
	if (/gtk_accel_path "([^"]*)" *"([^"]*)"/) 
		{
		my $name = $1;
		my $key = $2;
		$name =~ s/.*\///;
		$key =~ s/</&lt;/g;
		$key =~ s/>/&gt;/g;
		$funcs{uc($key)} = $name;
		}
		
	}
close(ACCELS);

open(ACCELS, "<hardcoded_keys") or die "No hardcoded_keys file";
while (<ACCELS>)
	{
	if (/"([^"]*)" *"([^"]*)"/) 
		{
		my $name = $1;
		my $key = $2;
		$name =~ s/.*\///;
		$key =~ s/</&lt;/g;
		$key =~ s/>/&gt;/g;
		$funcs{uc($key)} = $name;
		}
		
	}
close(ACCELS);

open(IN, "<keymap_template.svg") or die "No svg file";
open(OUT, ">keymap.svg") or die "Can't write output file";

while (<IN>)
	{
	if (/>key:([^<]*)</) 
		{
		my $key = uc($1);
		my $name = $funcs{$key};
		s/>key:([^<]*)</>$name</;
		delete $funcs{$key};
		}
	print OUT;
	}

close(IN);
close(OUT);

for my $key (keys %funcs)
	{
	if ($key)
		{
		print STDERR "not found: '$key'\n";
		}
	}

