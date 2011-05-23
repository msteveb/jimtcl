#
# Regular cron jobs for the jimtcl package
#
0 4	* * *	root	[ -x /usr/bin/jimtcl_maintenance ] && /usr/bin/jimtcl_maintenance
