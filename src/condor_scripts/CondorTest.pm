##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************


# CondorTest.pm - a Perl module for automated testing of Condor
#
# 19??-???-?? originally written by Tom Stanis (?)
# 2000-Jun-02 total overhaul by pfc@cs.wisc.edu and wright@cs.wisc.edu

package CondorTest;

require 5.0;
use Carp;
use Condor;
use CondorPersonal;
use FileHandle;
use POSIX "sys_wait_h";
use Net::Domain qw(hostfqdn);

my %securityoptions =
(
"NEVER" => "1",
"OPTIONAL" => "1",
"PREFERRED" => "1",
"REQUIRED" => "1",
);

BEGIN
{
    # disable command buffering so output is flushed immediately
    STDOUT->autoflush();
    STDERR->autoflush();

    $MAX_CHECKPOINTS = 2;
    $MAX_VACATES = 3;

    @skipped_output_lines = ( );
    @expected_output = ( );
    $checkpoints = 0;
	$hoststring = "notset:000";
    $vacates = 0;
    %test;
	%machine_ads;
	$lastconfig = "";

    Condor::DebugOn();
}

sub Reset
{
    %machine_ads = ();
	Condor::Reset();
	$hoststring = "notset:000";
}

sub ForceVacate
{
    my %info = @_;

    return 0 if ( $checkpoints >= $MAX_CHECKPOINTS ||
		  $vacates >= $MAX_VACATES );

    # let the job run for a few seconds and then send vacate signal
    sleep 5;
    Condor::Vacate( "\"$info{'sinful'}\"" );
    $vacates++;
    return 1;
}

sub RegisterSubmit
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "submit: missing function reference argument";

    $test{$handle}{"RegisterSubmit"} = $function_ref;
}
sub RegisterExecute
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "execute: missing function reference argument";

    $test{$handle}{"RegisterExecute"} = $function_ref;
}
sub RegisterEvicted
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "evict: missing function reference argument";

    $test{$handle}{"RegisterEvicted"} = $function_ref;
}
sub RegisterEvictedWithCheckpoint
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterEvictedWithCheckpoint"} = $function_ref;
}
sub RegisterEvictedWithRequeue
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterEvictedWithRequeue"} = $function_ref;
}
sub RegisterEvictedWithoutCheckpoint
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterEvictedWithoutCheckpoint"} = $function_ref;
}
sub RegisterExited
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterExited"} = $function_ref;
}
sub RegisterExitedSuccess
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "exit success: missing function reference argument";

    $test{$handle}{"RegisterExitedSuccess"} = $function_ref;
}
sub RegisterExitedFailure
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterExitedFailure"} = $function_ref;
}
sub RegisterExitedAbnormal
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterExitedAbnormal"} = $function_ref;
}
sub RegisterAbort
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterAbort"} = $function_ref;
}
sub RegisterShadow
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterShadow"} = $function_ref;
}
sub RegisterWantError
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterWantError"} = $function_ref;
}
sub RegisterHold
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterHold"} = $function_ref;
}
sub RegisterRelease
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterRelease"} = $function_ref;
}
sub RegisterJobErr
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterJobErr"} = $function_ref;
}

sub RegisterTimed
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";
	my $alarm = shift || croak "missing wait time argument";

    $test{$handle}{"RegisterTimed"} = $function_ref;
    $test{$handle}{"RegisterTimedWait"} = $alarm;

	# relook at registration and re-register to allow a timer
	# to be set after we are running. 
	# Prior to this change timed callbacks were only regsitered
	# when we call "runTest" and similar calls at the start.

	CheckTimedRegistrations();
}

sub RemoveTimed
{
    my $handle = shift || croak "missing handle argument";

    $test{$handle}{"RegisterTimed"} = undef;
    $test{$handle}{"RegisterTimedWait"} = undef;
    debug( "Remove timer.......\n",4);
    Condor::RemoveTimed( );
}

sub DefaultOutputTest
{
    my %info = @_;

    croak "default_output_test called but no \@expected_output defined"
	unless defined @expected_output;

    debug( "\$info{'output'} = $info{'output'}\n" ,4);

	my $output = "";
	my $error = "";
	my $initialdir = $info{'initialdir'};
	if($initialdir ne "") {
		debug( "Testing with initialdir = $initialdir\n" ,4);
		$output = $initialdir . "/" . $info{'output'};
		$error = $initialdir . "/" . $info{'error'};
		debug( "$output is now being tested.....\n" ,4);
	} else {
		$output = $info{'output'};
		$error = $info{'error'};
	}

    CompareText( $output, \@expected_output, @skipped_output_lines )
	|| die "$handle: FAILURE (STDOUT doesn't match expected output)\n";

    IsFileEmpty( $error ) || 
	die "$handle: FAILURE (STDERR contains data)\n";
}

sub RunTest
{
    $handle              = shift || croak "missing handle argument";
    my $submit_file      = shift || croak "missing submit file argument";
    my $wants_checkpoint = shift;

    my $status           = -1;
	my $monitorpid = 0;
	my $waitpid = 0;
	my $monitorret = 0;

	print "RunTest says test is<<$handle>>\n";
	# moved the reset to preserve callback registrations which includes
	# an error callback at submit time..... Had to change timing
	CondorTest::Reset();

    croak "too many arguments" if shift;

    # this is kludgey :: needed to happen sooner for an error message callback in runcommand
    $Condor::submit_info{'handle'} = $handle;

    # if we want a checkpoint, register a function to force a vacate
    # and register a function to check to make sure it happens
	if( $wants_checkpoint )
	{
		Condor::RegisterExecute( \&ForceVacate );
		Condor::RegisterEvictedWithCheckpoint( sub { $checkpoints++ } );
	} else {
		if(defined $test{$handle}{"RegisterExecute"}) {
			Condor::RegisterExecute($test{$handle}{"RegisterExecute"});
		}
	}

	CheckRegistrations();

	my $wrap_test = $ENV{WRAP_TESTS};

	my $config = "";
	if(defined  $wrap_test) {
		print "Config before PersonalCondorTest<<<<$ENV{CONDOR_CONFIG}>>>>\n";
		$lastconfig = $ENV{CONDOR_CONFIG};
		$config = PersonalCondorTest($submit_file);
		print "PersonalCondorTest returned this config file<$config>\n";
		print "Saving old config file <<<$lastconfig>>>\n";
		$ENV{CONDOR_CONFIG} = $config;
	}

    # submit the job and get the cluster id
    $cluster = Condor::TestSubmit( $submit_file );
    
    # if condor_submit failed for some reason return an error
    if($cluster == 0){
	} else {

    	# monitor the cluster and return its exit status
		# note 1/2/09 bt
		# any exits cause monitor to never return allowing us
		# to kill personal condor wrapping the test :-(
		
		$monitorpid = fork();
		if($monitorpid == 0) {
			# child does monitor
    		$monitorret = Condor::Monitor();

			debug( "Monitor did return on its own status<<<$monitorret>>>\n",4);
    		die "$handle: FAILURE (job never checkpointed)\n"
			if $wants_checkpoint && $checkpoints < 1;

			if(  $monitorret == 1 ) {
				debug( "child happy to exit 0\n",4);
				exit(0);
			} else {
				debug( "child not happy to exit 1\n",4);
				exit(1);
			}
		} else {
			# parent cleans up
			$waitpid = waitpid($monitorpid, 0);
			if($waitpid == -1) {
				debug( "No such process <<$monitorpid>>\n",4);
			} else {
				$retval = $?;
				debug( "Child status was <<$retval>>\n",4);
				if( WIFEXITED( $retval ) && WEXITSTATUS( $retval ) == 0 )
				{
					debug( "Monitor done and status good!\n",4);
					$retval = 1;
				} else {
					my $status = WEXITSTATUS( $retval );
					debug( "Monitor done and status bad<<$status>>!\n",4);
					$retval = 0;
				}
			}
		}
	}

	debug( "************** condor_monitor back ************************ \n",4);

	if(defined  $wrap_test) {
		if($config ne "") {
			print "KillDaemonPids called on this config file<$config>\n";
			CondorPersonal::KillDaemonPids($config);
		} else {
			print "No config setting to call KillDaemonPids with\n";
		}
		print "Restoring this config<<<$lastconfig>>>\n";
		$ENV{CONDOR_CONFIG} = $lastconfig;
	} else {
		debug( "Not currently wrapping tests\n",4);
	}


    if($cluster == 0){
		return(0);
	} else {
    	return $retval;
	}
}

sub RunDagTest
{
    $handle              = shift || croak "missing handle argument";
    my $submit_file      = shift || croak "missing submit file argument";
    my $wants_checkpoint = shift;
	my $dagman_args = 	shift || croak "missing dagman args";

    my $status           = -1;
	my $monitorpid = 0;
	my $waitpid = 0;
	my $monitorret = 0;

    croak "too many arguments" if shift;

	# moved the reset to preserve callback registrations which includes
	# an error callback at submit time..... Had to change timing
	CondorTest::Reset();

    # this is kludgey :: needed to happen sooner for an error message callback in runcommand
    $Condor::submit_info{'handle'} = $handle;

    # if we want a checkpoint, register a function to force a vacate
    # and register a function to check to make sure it happens
	if( $wants_checkpoint )
	{
		Condor::RegisterExecute( \&ForceVacate );
		Condor::RegisterEvictedWithCheckpoint( sub { $checkpoints++ } );
	} else {
		if(defined $test{$handle}{"RegisterExecute"}) {
			Condor::RegisterExecute($test{$handle}{"RegisterExecute"});
		}
	}

	CheckRegistrations();

	my $wrap_test = $ENV{WRAP_TESTS};

	my $config = "";
	if(defined  $wrap_test) {
		$lastconfig = $ENV{CONDOR_CONFIG};
		$config = PersonalCondorTest($submit_file);
		print "PersonalCondorTest returned this config file<$config>\n";
		print "Saving last config file<<<$lastconfig>>>\n";
		$ENV{CONDOR_CONFIG} = $config;
	}

    # submit the job and get the cluster id
    $cluster = Condor::TestSubmitDagman( $submit_file, $dagman_args );
    
	if($cluster == 0){
    	# if condor_submit failed for some reason return an error
	} else {
    	# monitor the cluster and return its exit status
		# note 1/2/09 bt
		# any exits cause monitor to never return allowing us
		# to kill personal condor wrapping the test :-(
		
		$monitorpid = fork();
		if($monitorpid == 0) {
			# child does monitor
    		$monitorret = Condor::Monitor();
			debug( "Monitor did return on its own status<<<$monitorret>>>\n",4);

    		die "$handle: FAILURE (job never checkpointed)\n"
			if $wants_checkpoint && $checkpoints < 1;

			if(  $monitorret == 1 ) {
				debug( "child happy to exit 0\n",4);
				exit(0);
			} else {
				debug( "child not happy to exit 1\n",4);
				exit(1);
			}
		} else {
			# parent cleans up
			$waitpid = waitpid($monitorpid, 0);
			if($waitpid == -1) {
				debug( "No such process <<$monitorpid>>\n",4);
			} else {
				$retval = $?;
				debug( "Child status was <<$retval>>\n",4);
				if( WIFEXITED( $retval ) && WEXITSTATUS( $retval ) == 0 )
				{
					debug( "Monitor done and status good!\n",4);
					$retval = 1;
				} else {
					my $status = WEXITSTATUS( $retval );
					debug( "Monitor done and status bad<<$status>>!\n",4);
					$retval = 0;
				}
			}
		}
	}

	debug( "************** condor_monitor back ************************ \n",4);

	if(defined  $wrap_test) {
		if($config ne "") {
			print "KillDaemonPids called on this config file<$config>\n";
			CondorPersonal::KillDaemonPids($config);
		} else {
			print "No config setting to call KillDaemonPids with\n";
		}
		print "Restoring last config file<<$lastconfig>>\n";
		$ENV{CONDOR_CONFIG} = $lastconfig;
	} else {
		debug( "Not currently wrapping tests\n",4);
	}


	if($cluster == 0){
		return(0);
	} else {
		return $retval;
	}
}

sub CheckTimedRegistrations
{
	# this one event should be possible from ANY state
	# that the monitor reports to us. In this case which
	# initiated the change I wished to start a timer from
	# when the actual runtime was reached and not from
	# when we started the test and submited it. This is the time 
	# at all other regsitrations have to be registered by....

    if( defined $test{$handle}{"RegisterTimed"} )
    {
		debug( "Found a timer to regsiter.......\n",4);
		Condor::RegisterTimed( $test{$handle}{"RegisterTimed"} , $test{$handle}{"RegisterTimedWait"});
    }
}

sub CheckRegistrations
{
    # any handle-associated functions with the cluster
    # or else die with an unexpected event
    if( defined $test{$handle}{"RegisterExitedSuccess"} )
    {
        Condor::RegisterExitSuccess( $test{$handle}{"RegisterExitedSuccess"} );
    }
    else
    {
	Condor::RegisterExitSuccess( sub {
	    die "$handle: FAILURE (got unexpected successful termination)\n";
	} );
    }

    if( defined $test{$handle}{"RegisterExitedFailure"} )
    {
	Condor::RegisterExitFailure( $test{$handle}{"RegisterExitedFailure"} );
    }
    else
    {
	Condor::RegisterExitFailure( sub {
	    my %info = @_;
	    die "$handle: FAILURE (returned $info{'retval'})\n";
	} );
    }

    if( defined $test{$handle}{"RegisterExitedAbnormal"} )
    {
	Condor::RegisterExitAbnormal( $test{$handle}{"RegisterExitedAbnormal"} );
    }
    else
    {
	Condor::RegisterExitAbnormal( sub {
	    my %info = @_;
	    die "$handle: FAILURE (got signal $info{'signal'})\n";
	} );
    }

    if( defined $test{$handle}{"RegisterShadow"} )
    {
	Condor::RegisterShadow( $test{$handle}{"RegisterShadow"} );
    }

    if( defined $test{$handle}{"RegisterWantError"} )
    {
	Condor::RegisterWantError( $test{$handle}{"RegisterWantError"} );
    }

    if( defined $test{$handle}{"RegisterAbort"} )
    {
	Condor::RegisterAbort( $test{$handle}{"RegisterAbort"} );
    }
    else
    {
	Condor::RegisterAbort( sub {
	    my %info = @_;
	    die "$handle: FAILURE (job aborted by user)\n";
	} );
    }

    if( defined $test{$handle}{"RegisterHold"} )
    {
	Condor::RegisterHold( $test{$handle}{"RegisterHold"} );
    }
    else
    {
	Condor::RegisterHold( sub {
	    my %info = @_;
	    die "$handle: FAILURE (job held by user)\n";
	} );
    }

    if( defined $test{$handle}{"RegisterSubmit"} )
    {
	Condor::RegisterSubmit( $test{$handle}{"RegisterSubmit"} );
    }

    if( defined $test{$handle}{"RegisterRelease"} )
    {
	Condor::RegisterRelease( $test{$handle}{"RegisterRelease"} );
    }
    #else
    #{
	#Condor::RegisterRelease( sub {
	    #my %info = @_;
	    #die "$handle: FAILURE (job released by user)\n";
	#} );
    #}

    if( defined $test{$handle}{"RegisterJobErr"} )
    {
	Condor::RegisterJobErr( $test{$handle}{"RegisterJobErr"} );
    }
    else
    {
	Condor::RegisterJobErr( sub {
	    my %info = @_;
	    die "$handle: FAILURE (job error -- see $info{'log'})\n";
	} );
    }

    # if we wanted to know about requeues.....
    if( defined $test{$handle}{"RegisterEvictedWithRequeue"} )
    {
        Condor::RegisterEvictedWithRequeue( $test{$handle}{"RegisterEvictedWithRequeue"} );
    } 

    # if evicted, call condor_resched so job runs again quickly
    if( !defined $test{$handle}{"RegisterEvicted"} )
    {
        Condor::RegisterEvicted( sub { sleep 5; Condor::Reschedule } );
    } else {
	Condor::RegisterEvicted( $test{$handle}{"RegisterEvicted"} );
    }

    if( defined $test{$handle}{"RegisterTimed"} )
    {
		Condor::RegisterTimed( $test{$handle}{"RegisterTimed"} , $test{$handle}{"RegisterTimedWait"});
    }
}


sub CompareText
{
    my $file = shift || croak "missing file argument";
    my $aref = shift || croak "missing array reference argument";
    my @skiplines = @_;
    my $linenum = 0;

	debug("opening file $file to compare to array of expected results\n",4);
    open( FILE, "<$file" ) || die "error opening $file: $!\n";
    
    while( <FILE> )
    {
	fullchomp($_);
	$line = $_;
	$linenum++;

	debug("linenum $linenum\n",4);
	debug("\$line: $line\n",4);
	debug("\$\$aref[0] = $$aref[0]\n",4);

	debug("skiplines = \"@skiplines\"\n",4);
	#print "grep returns ", grep( /^$linenum$/, @skiplines ), "\n";

	next if grep /^$linenum$/, @skiplines;

	$expectline = shift @$aref;
	if( ! defined $expectline )
	{
	    die "$file contains more text than expected\n";
	}
	fullchomp($expectline);

	debug("\$expectline: $expectline\n",4);

	# if they match, go on
	next if $expectline eq $line;

	# otherwise barf
	warn "$file line $linenum doesn't match expected output:\n";
	warn "actual: $line\n";
	warn "expect: $expectline\n";
	return 0;
    }
	close(FILE);

    # barf if we're still expecting text but the file has ended
    ($expectline = shift @$aref ) && 
        die "$file incomplete, expecting:\n$expectline\n";

    # barf if there are skiplines we haven't hit yet
    foreach $num ( @skiplines )
    {
	if( $num > $linenum )
	{
	    warn "skipline $num > # of lines in $file ($linenum)\n";
	    return 0;
	}
	croak "invalid skipline argument ($num)" if $num < 1;
    }
    
	debug("CompareText successful\n",4);
    return 1;
}

sub IsFileEmpty
{
    my $file = shift || croak "missing file argument";
    return -z $file;
}

sub verbose_system 
{
	my $args = shift @_;


	my $catch = "vsystem$$";
	$args = $args . " 2>" . $catch;
	my $rc = 0xffff & system $args;

	printf "system(%s) returned %#04x: ", $args, $rc;

	if ($rc == 0) 
	{
		print "ran with normal exit\n";
		return $rc;
	}
	elsif ($rc == 0xff00) 
	{
		print "command failed: $!\n";
	}
	elsif (($rc & 0xff) == 0) 
	{
		$rc >>= 8;
		print "ran with non-zero exit status $rc\n";
	}
	else 
	{
		print "ran with ";
		if ($rc &   0x80) 
		{
			$rc &= ~0x80;
			print "coredump from ";
		}
		print "signal $rc\n"
	}

	if( !open( MACH, "<$catch" )) { 
		warn "Can't look at command  output <$catch>:$!\n";
	} else {
    	while(<MACH>) {
        	print "ERROR: $_";
    	}
    	close(MACH);
	}

	return $rc;
}

sub MergeOutputFiles
{
	my $Testhash = shift || croak "Missing Test hash to Merge Output\n";
	my $basename = $Testhash->{corename};

	foreach my $m ( 0 .. $#{$Testhash->{extensions}} )
	{
		my $newlog = $basename . $Testhash->{extensions}[$m];
		#print "Creating core log $newlog\n";
		open(LOG,">$newlog") || return "1";
		print LOG "***************************** Merge Sublogs ***************************\n";
		foreach my $n ( 0 .. $#{$Testhash->{tests}} )
		{
			# get file if it exists
			#print "Add logs for test $Testhash->{tests}[$n]\n";
			my $sublog = $Testhash->{tests}[$n] . $Testhash->{extensions}[$m];
			if( -e "$sublog" )
			{
				print LOG "\n\n***************************** $sublog ***************************\n\n";
				open(INLOG,"<$sublog") || return "1";
				while(<INLOG>)
				{
					print LOG "$_";
				}
				close(INLOG);
			}
			else
			{
				#print "Can not find $sublog\n";
			}
			#print "$n = $Testhash->{tests}[$n]\n";
			#print "$m = $Testhash->{extensions}[$m]\n";
		}
		close(LOG);
	}
}

sub ParseMachineAds
{
    my $machine = shift || croak "missing machine argument";
    my $line = 0;

	if( ! open(PULL, "condor_status -l $machine 2>&1 |") )
    {
		print "error getting Ads for \"$machine\": $!\n";
		return 0;
    }
    
    debug( "reading machine ads from $machine...\n" ,5);
    while( <PULL> )
    {
	fullchomp($_);
	debug("Raw AD is $_\n",5);
	$line++;

	# skip comments & blank lines
	next if /^#/ || /^\s*$/;

	# if this line is a variable assignment...
	if( /^(\w+)\s*\=\s*(.*)$/ )
	{
	    $variable = lc $1;
	    $value = $2;

	    # if line ends with a continuation ('\')...
	    while( $value =~ /\\\s*$/ )
	    {
		# remove the continuation
		$value =~ s/\\\s*$//;

		# read the next line and append it
		<PULL> || last;
		$value .= $_;
	    }

	    # compress whitespace and remove trailing newline for readability
	    $value =~ s/\s+/ /g;
	    fullchomp($value);

	
		# Do proper environment substitution
	    if( $value =~ /(.*)\$ENV\((.*)\)(.*)/ )
	    {
			my $envlookup = $ENV{$2};
	    	debug( "Found $envlookup in environment \n",5);
			$value = $1.$envlookup.$3;
	    }

	    debug( "$variable = $value\n" ,5);
	    
	    # save the variable/value pair
	    $machine_ads{$variable} = $value;
	}
	else
	{
	    debug( "line $line of $submit_file not a variable assignment... " .
		   "skipping\n" ,5);
	}
    }
	close(PULL);
    return 1;
}

sub FetchMachineAds
{
	return %machine_ads;
}

sub FetchMachineAdValue
{
	my $key = shift @_;
	if(exists $machine_ads{$key})
	{
		return $machine_ads{$key};
	}
	else
	{
		return undef;
	}
}

#
# Some tests need to wait to be started and as such we will
# use qedit to change the job add.
#

sub setJobAd
{
	my @status;
	my $qstatcluster = shift;
	my $qattribute = shift; # change which job ad?
	my $qvalue = shift;		# whats the new value?
	my $qtype = shift;		# quote if a string...
	my $cmd = "condor_qedit $qstatcluster $qattribute ";
	if($qtype eq "string") {
		$cmd = $cmd . "\"$qvalue\"";
	} else {
		$cmd = $cmd . "$qvalue";
	}
	print "Running this command: <$cmd> \n";
	# shhhhhhhh third arg 0 makes it hush its output
	my $qstat = CondorTest::runCondorTool($cmd,\@status,0);
	if(!$qstat)
	{
		print "Test failure due to Condor Tool Failure<$cmd>\n";
	    return(1)
	}
	foreach $line (@status)
	{
		#print "Line: $line\n";
	}
}

#
# Is condor_q able to respond at this time? We'd like to get
# a valid response for whoever is asking.
#

sub getJobStatus
{
	my @status;
	my $qstatcluster = shift;
	my $cmd = "condor_q $qstatcluster -format %d JobStatus";
	# shhhhhhhh third arg 0 makes it hush its output
	my $qstat = CondorTest::runCondorTool($cmd,\@status,0);
	if(!$qstat)
	{
		print "Test failure due to Condor Tool Failure<$cmd>\n";
	    return(1)
	}

	foreach $line (@status)
	{
		#print "jobstatus: $line\n";
		if( $line =~ /^(\d).*/)
		{
			return($1);
		}
		else
		{
			return(-1);
		}
	}
}

#
# Run a condor tool and look for exit value. Apply multiplier
# upon failure and return 1 on failure.
#

sub runCondorTool
{
	my $trymultiplier = 1;
	my $start_time = time; #get start time
	my $delta_time = 0;
	my $status = 1;
	my $done = 0;
	my $cmd = shift;
	my $arrayref = shift;
	# use unused third arg to skip the noise like the time
	my $quiet = shift;
	my $force = "";
	$force = shift;
	my $count = 0;
	my $catch = "runCTool$$";

	# clean array before filling
	if($quiet != 0) {
		system("date");
	}

	my $attempts = 8;
	my $count = 0;
	while( $count < $attempts) {
		@{$arrayref} = (); #empty return array...
		my @tmparray;
		debug( "Try command <$cmd>\n",4);
		open(PULL, "$cmd 2>$catch |");
		while(<PULL>)
		{
			fullchomp($_);
			debug( "Process: $_\n",4);
			push @tmparray, $_; # push @{$arrayref}, $_;
		}
		close(PULL);
		$status = $? >> 8;
		debug("Status is $status after command\n",4);
		if(( $status != 0 ) && ($attempts == ($count + 1)))
		{
				print "runCondorTool: $cmd timestamp $start_time failed!\n";
				print "************* std out ***************\n";
				foreach $stdout (@tmparray) {
					print "STDOUT: $stdout \n";
				}
				print "************* std err ***************\n";
				if( !open( MACH, "<$catch" )) { 
					warn "Can't look at command output <$catch>:$!\n";
				} else {
    				while(<MACH>) {
        				print "ERROR: $_";
    				}
    				close(MACH);
				}
				print "************* GetQueue() ***************\n";
				GetQueue();
				print "************* GetQueue() DONE ***************\n";

				return(0);
		}

		if ($status == 0) {
			$line = "";
			foreach $value (@tmparray)
			{
				push @{$arrayref}, $value;
			}
			$done = 1;
			# There are times like the security tests when we want
			# to see the stderr even when the command works.
			if( $force ne "" ) {
				if( !open( MACH, "<$catch" )) { 
					warn "Can't look at command output <$catch>:$!\n";
				} else {
    				while(<MACH>) {
						fullchomp($_);
						$line = $_;
						push @{$arrayref}, $line;
    				}
    				close(MACH);
				}
			}
			my $current_time = time;
			$delta_time = $current_time - $start_time;
			debug("runCondorTool: its been $delta_time since call\n",4);
			return(1);
		}
		$count = $count + 1;
		debug("runCondorTool: iteration<$count> failed sleep 10 * $count \n",1);
		sleep((10*$count));
	}
	debug( "runCondorTool: $cmd worked!\n",4);

	return(0);
}

# Sometimes `which ...` is just plain broken due to stupid fringe vendor
# not quite bourne shells. So, we have our own implementation that simply
# looks in $ENV{"PATH"} for the program and return the "usual" response found
# across unicies. As for windows, well, for now it just sucks.
sub Which
{
	my $exe = shift(@_);
	my @paths = split /:/, $ENV{'PATH'};
	my $path;

	foreach $path (@paths) {
		chomp $path;
		if (-x "$path/$exe") {
			return "$path/$exe";
		}
	}

	return "$exe: command not found";
}

# Lets be able to drop some extra information if runCondorTool
# can not do what it is supposed to do....... short and full
# output from condor_q 11/13

sub GetQueue
{
	my @cmd = ("condor_q", "condor_q -l" );
	foreach $request (@cmd) {
		print "Queue command <$request>\n";
		open(PULL, "$request 2>&1 |");
		while(<PULL>)
		{
			fullchomp($_);
			print "GetQueue: $_\n";
		}
		close(PULL);
	}
}

#
# Cygwin's perl chomp does not remove cntrl-m but this one will
# and linux and windows can share the same code. The real chomp
# totals the number or changes but I currently return the modified
# array. bt 10/06
#

sub fullchomp
{
	push (@_,$_) if( scalar(@_) == 0);
	foreach my $arg (@_) {
		$arg =~ s/\012+$//;
		$arg =~ s/\015+$//;
	}
	return(0);
}

sub changeDaemonState
{
	my $timeout = 0;
	my $daemon = shift;
	my $state = shift;
	$timeout = shift; # picks up number of tries... back off on how soon we try.
	my $counter = 0;
	my $cmd = "";
	my $foundTotal = "no";
	my $status;
	my @cmdarray1, @cmdarray2;

	print "Checking for $daemon being $state\n";
	if($state eq "off") {
		$cmd = "condor_off -fast -$daemon";
	} elsif($state eq "on") {
		$cmd = "condor_on -$daemon";
	} else {
		die "Bad state given in changeScheddState<$state>\n";
	}

	$status = runCondorTool($cmd,\@cmdarray1,2);
	if(!$status)
	{
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		exit(1);
	}

	my $sleeptime = 0;
	$cmd = "condor_status -$daemon";
	while($counter < $timeout ) {
		$foundTotal = "no";
		@cmdarray2 = {};
		print "about to run $cmd try $counter previous sleep $sleeptime\n";
		$status = CondorTest::runCondorTool($cmd,\@cmdarray2,2);
		if(!$status)
		{
			print "Test failure due to Condor Tool Failure<$cmd>\n";
			exit(1)
		}

		foreach my $line (@cmdarray2)
		{
			print "<<$line>>\n";
			if($daemon eq "schedd") {
				if( $line =~ /.*Total.*/ ) {
					# hmmmm  scheduler responding
					print "Schedd running\n";
					$foundTotal = "yes";
				}
			} elsif($daemon eq "startd") {
				if( $line =~ /.*Backfill.*/ ) {
					# hmmmm  Startd responding
					print "Startd running\n";
					$foundTotal = "yes";
				}
			}
		}

		if( $state eq "on" ) {
			if($foundTotal eq "yes") {
				# is running again
				return(1);
			} else {
				$counter = $counter + 1;
				$sleeptime = ($counter**2);
				sleep($sleeptime);
			}
		} elsif( $state eq "off" ) {
			if($foundTotal eq "no") {
				#is stopped
				return(1);
			} else {
				$counter = $counter + 1;
				$sleeptime = ($counter**2);
				sleep($sleeptime);
			}
		}

	}
	print "Timeout watching for $daemon state change to <<$state>>\n";
	return(0);
}

#######################
#
# find_pattern_in_array
#
#	Find the array index which contains a particular pattern.
#
# 	First used to strip off variant line in output from
#	condor_q -direct when passed quilld, schedd and rdbms
#	prior to comparing the arrays collected from the output
#	of each command....

sub find_pattern_in_array
{
    $pattern = shift;
    $harray = shift;
    $place = 0;

    debug( "Looking for <<$pattern>> size <<$#{$harray}>>\n",4);
    foreach $member (@{$harray}) {
        debug( "consider $member\n",5);
        if($member =~ /.*$pattern.*/) {
            debug( "Found <<$member>> line $place\n",4);
            return($place);
        } else {
            $place = $place + 1;
        }
    }
    print "Got to end without finding it....<<$pattern>>\n";
    return(-1);
}

#######################
#
# compare_arrays
#
#	We hash each line from an array and verify that each array has the same contents
# 	by having a value for each key equalling the number of arrays. First used to
#	compare output from condor_q -direct quilld, schedd and rdbms

sub compare_arrays
{
    $startrow = shift;
    $endrow = shift;
    $numargs = shift;
    %lookup = ();
    $counter = 0;
    debug( "Check $numargs starting row $startrow end row $endrow\n",4);
    while($counter < $numargs) {
        $href = shift;
        my $thisrow = 0;
        for $item (@{$href}) {
            if( $thisrow >= $startrow) {
                if($counter == 0) {
                    #initialize each position
                    $lookup{$item} = 1;
                } else {
                    $lookup{$item} = $lookup{$item} + 1;
                    debug( "Set at:<$lookup{$item}:$item>\n",4);
                }
                debug( "Store: $item\n",5);
            } else {
                debug( "Skip: $item\n",4);
            }
            $thisrow = $thisrow + 1;
        }
        $counter = $counter + 1;
    }
    #loaded up..... now look!
    foreach $key (keys %lookup) {
        debug( " $key equals $lookup{$key}\n",4);
		if($lookup{$key} != $numargs) {
			print "Arrays are not the same! key <$key> is $lookup{$key} and not $numargs\n";
			return(1);
		}
    }
	return(0);
}

##############################################################################
#
# spawn_cmd
#
#	For a process to start two processes. One will start the passed system
# 	call and the other will wait around for completion where it will stuff 
# 	the results where we can check when we care
#
##############################################################################

sub spawn_cmd
{
	my $cmdtowatch = shift;
	my $resultfile = shift;
	my $result;
	my $toppid = fork();

	if($toppid == 0) {

		$pid = fork();
		if ($pid == 0) {
			# child 1 code....
			$mylog = $resultfile . ".spawn";
			open(LOG,">$mylog") || die "Can not open log: $mylog: $!\n";
			my $res = 0;
			print LOG "Starting this cmd <$cmdtowatch>\n";
			$res = system("$cmdtowatch");
			print LOG "Result from $cmdtowatch is <$res>\n";
			print LOG "File to watch is <$resultfile>\n";
			if($res != 0) {
				print LOG " failed\n";
				close(LOG);
				exit(-1);
			} else {
				print LOG " worked\n";
				close(LOG);
				exit(0);
			}
		} 

		open(RES,">$resultfile") || die "Can't open results file<$resultfile>:$!\n";
		$mylog = $resultfile . ".watch";
		open(LOG,">$mylog") || die "Can not open log: $mylog: $!\n";
		print LOG "waiting on pid <$pid>\n";
		while(($child = waitpid($pid,WNOHANG)) != -1) { 
			$retval = $?;
			debug( "Child status was <<$retval>>\n",4);
			if( WIFEXITED( $retval ) && WEXITSTATUS( $retval ) == 0 ) {
				debug( "Monitor done and status good!\n",4);
				$retval = 1;
			} else {
				my $status = WEXITSTATUS( $retval );
				debug( "Monitor done and status bad<<$status>>!\n",4);
				$retval = 0;
			}

			#$exitval = $? >> 8;
			#$signal_num = $? & 127;
			print RES "Exit $retval \n";
			print LOG "Pid $child res was $retval\n";
		}
		print LOG "Done waiting on pid <$pid>\n";
		close(RES);
		close(LOG);

		exit(0);
	} else {
		# we are done
		return($toppid);
	}
}

##############################################################################
#
# getFqdnHost
#
# hostname sometimes does not return a fully qualified
# domain name and we must ensure we have one. We will call
# this function in the tests wherever we were calling
# hostname.
##############################################################################

sub getFqdnHost
{
	my $host = hostfqdn();
	return($host);
}

##############################################################################
#
# PersonalSearchLog
#
# Serach a log for a pattern
#
##############################################################################

sub PersonalSearchLog
{
    my $pid = shift;
    my $personal = shift;
    my $searchfor = shift;
    my $logname = shift;

	my $logdir = `condor_config_val log`;
	fullchomp($logdir);

    #my $logloc = $pid . "/" . $pid . $personal . "/log/" . $logname;
    my $logloc = $logdir . "/" . $logname;
    CondorTest::debug("Search this log <$logloc> for <$searchfor>\n",1);
    open(LOG,"<$logloc") || die "Can not open logfile<$logloc>: $!\n";
    while(<LOG>) {
        if( $_ =~ /$searchfor/) {
            CondorTest::debug("FOUND IT! $_",1);
            return(0);
        }
    }
    return(1);
}

##############################################################################
#
# PersonalPolicySearchLog
#
# Serach a log for a security policy
#
##############################################################################


sub PersonalPolicySearchLog
{
    my $pid = shift;
    my $personal = shift;
    my $policyitem = shift;
    my $logname = shift;

	my $logdir = `condor_config_val log`;
	fullchomp($logdir);

    #my $logloc = $pid . "/" . $pid . $personal . "/log/" . $logname;
    my $logloc = $logdir . "/" . $logname;
    debug("Search this log <$logloc> for <$policyitem>\n",1);
    open(LOG,"<$logloc") || die "Can not open logfile<$logloc>: $!\n";
    while(<LOG>) {
        if( $_ =~ /^.*Security Policy.*$/) {
            while(<LOG>) {
                if( $_ =~ /^\s*$policyitem\s*=\s*\"(\w+)\"\s*$/ ) {
                    #print "FOUND IT! $1\n";
                    if(!defined $securityoptions{$1}){
                        debug("Returning <<$1>>\n",1);
                        return($1);
                    }
                }
            }
        }
    }
    return("bad");
}

sub PersonalCondorTest
{
	my $submitfile = shift;
	my $cmd = "condor_config_val log";
	my $locconfig = "";
    print "Running this command: <$cmd> \n";
    # shhhhhhhh third arg 0 makes it hush its output
	$logdir = `condor_config_val log`;
	fullchomp($logdir);
	print "log dir is<$logdir>\n";
	if($logdir =~ /^.*condor_tests.*$/){
		print "Running within condor_tests\n";
		if($logdir =~ /^.*TestingPersonalCondor.*$/){
			print "Running with outer testing personal condor\n";
			my $testname = findOutput($submitfile);
			print "findOutput saya test is $testname\n";
			my $version = "local";
			
			# get a local scheduler running (side a)
			my $configloc = CondorPersonal::StartCondor( $testname, "x_param.basic_personal" ,$version);
			my @local = split /\+/, $configloc;
			$locconfig = shift @local;
			my $locport = shift @local;
			
			debug("---local config is $locconfig and local port is $locport---\n",1);

			#$ENV{CONDOR_CONFIG} = $locconfig;
		}
	} else {
		print "Running outside of condor_tests\n";
	}
	return($locconfig);
}

sub findOutput
{
	my $submitfile = shift;
	open(SF,"<$submitfile") or die "Failed to open <$submitfile>:$!\n";
	my $testname = "UNKNOWN";
	$line = "";
	while(<SF>) {
		chomp($_);
		$line = $_;
		if($line =~ /^\s*[Ll]og\s+=\s+(.*)(\..*)$/){
			$testname = $1;
			$previouslog = $1 . $2;
			system("rm -f $previouslog");
		}
	}
	close(SF);
	print "findOutput returning <$testname>\n";
	if($testname eq "UNKNOWN") {
		print "failed to find testname in this submit file:$submitfile\n";
		system("cat $submitfile");
	}
	return($testname);
}

# Call down to Condor Perl Module for now

sub debug
{
    my $string = shift;
	my $level = shift;
	my $newstring = "CT:$string";
	Condor::debug($newstring,$level);
}

1;
