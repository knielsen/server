package My::Suite::Metadata_snc_lock_info;

@ISA = qw(My::Suite);

return "No Metadata_snc_lock_info plugin" unless $ENV{METADATA_SNC_LOCK_INFO_SO} or
  $::mysqld_variables{'metadata-snc-lock-info'} eq "ON";;

sub is_default { 1 }

bless { };

