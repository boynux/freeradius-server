
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/time.h>
#include	<sys/file.h>
#include	<sys/stat.h>
#include	<netinet/in.h>

#include	<stdio.h>
#include	<stdlib.h>
#include	<netdb.h>
#include	<pwd.h>
#include	<time.h>
#include	<ctype.h>
#include	<unistd.h>
#include	<signal.h>
#include	<errno.h>
#include	<sys/wait.h>

#include	"rlm_sql.h"
#include	"radiusd.h"





/*************************************************************************
 *
 *	Function: sql_save_acct
 *
 *	Purpose: Write data from the sqlrecord structure to the database
 *
 *************************************************************************/

int sql_save_acct(SQLREC *sqlrecord) {

	char		querystr[2048];
	FILE		*sqlfile;
	FILE		*backupfile;
	int		num = 0;
	SQL_RES		*result;
#ifdef NT_DOMAIN_HACK
	char		*ptr;
	char		newname[AUTH_STRING_LEN];
#endif
	


     if((sqlfile = fopen(QUERYLOG, "a")) == (FILE *)NULL) {
            log(L_ERR, "Acct: Couldn't open file %s", QUERYLOG);
     } else { 
        #if defined(F_LOCK) && !defined(BSD)
              (void)lockf((int)sqlfile, (int)F_LOCK, (off_t)SQL_LOCK_LEN);
        #else
              (void)flock(sqlfile, SQL_LOCK_EX);
        #endif
     }

#ifdef NT_DOMAIN_HACK
	/*
	 *      Windows NT machines often authenticate themselves as
	 *      NT_DOMAIN\username. Try to be smart about this.
	 *
	 *      FIXME: should we handle this as a REALM ?
	 */
	if ((ptr = strchr(sqlrecord->UserName, '\\')) != NULL) {
		strncpy(newname, ptr + 1, sizeof(newname));
		newname[sizeof(newname) - 1] = 0;
		strcpy(sqlrecord->UserName, newname);
	}
#endif /* NT_DOMAIN_HACK */

 if (sql_checksocket("Acct")) {

     if (sqlrecord->AcctStatusTypeId == PW_STATUS_ACCOUNTING_ON || sqlrecord->AcctStatusTypeId == PW_STATUS_ACCOUNTING_OFF) {
        log(L_INFO, "Portmaster %s rebooted at %s", sqlrecord->NASIPAddress, sqlrecord->AcctTimeStamp);
  
         /* The Terminal server informed us that it was rebooted
         * STOP all records from this NAS */

         sprintf(querystr, "UPDATE %s SET AcctStopTime='%s', AcctSessionTime=unix_timestamp('%s') - unix_timestamp(AcctStartTime), AcctTerminateCause='%s' WHERE AcctSessionTime=0 AND AcctStopTime=0 AND NASIPAddress= '%s' AND AcctStartTime <= '%s'", sql->config.sql_acct_table, sqlrecord->AcctTimeStamp, sqlrecord->AcctTimeStamp, sqlrecord->AcctTerminateCause, sqlrecord->NASIPAddress, sqlrecord->AcctTimeStamp);

       	 if (sql_query(sql->AcctSock, (const char *) querystr) < 0)
	      log(L_ERR, "Acct: Couldn't update SQL accounting after NAS reboot - %s", sql_error(sql->AcctSock));

         if (sqlfile) {
              fputs(querystr, sqlfile);
              fputs(";\n", sqlfile);
              fclose(sqlfile);
          }
          return 0;
      } 

	if (sqlrecord->AcctStatusTypeId == PW_STATUS_ALIVE) {
		sprintf(querystr, "UPDATE %s SET Framed-IP-Address = '%s' WHERE AcctSessionId = '%s' AND UserName = '%s' AND NASIPAddress= '%s'", sql->config.sql_acct_table, sqlrecord->FramedIPAddress, sqlrecord->AcctSessionId, sqlrecord->UserName, sqlrecord->NASIPAddress);
		if (sql_query(sql->AcctSock, (const char *) querystr) < 0)
		log(L_ERR, "Acct: Couldn't update SQL accounting after NAS reboot - %s", sql_error(sql->AcctSock));

		if (sqlfile) {
			fputs(querystr, sqlfile);
			fputs(";\n", sqlfile);
			fclose(sqlfile);
		}
		return 0;
	}


          /* Got start record */
          if(sqlrecord->AcctStatusTypeId == PW_STATUS_START) {
             
             /* Set start time on record with only a stop record */
 	     snprintf(querystr, 2048, "UPDATE %s SET AcctStartTime = '%s' WHERE AcctSessionId = '%s' AND UserName = '%s' AND NASIPAddress = '%s'", 
	     sql->config.sql_acct_table,
             sqlrecord->AcctTimeStamp,
             sqlrecord->AcctSessionId,
             sqlrecord->UserName,
             sqlrecord->NASIPAddress
             );
       	     if (sql_query(sql->AcctSock, (const char *) querystr) < 0)
	        log(L_ERR, "Acct: Couldn't update SQL accounting START record - %s", sql_error(sql->AcctSock));

             num = sql_affected_rows(sql->AcctSock);
             if (num == 0) {

                /* Insert new record with blank stop time until stop record is got */
                snprintf(querystr, 2048, "INSERT INTO %s VALUES (0, '%s', '%s', '%s', '%s', %ld, '%s', '%s', 0, 0, '%s', '%s', 0, 0, '%s', '%s', '', '%s', '%s', '%s', %ld)",
                sql->config.sql_acct_table,
                sqlrecord->AcctSessionId,
                sqlrecord->UserName,
                sqlrecord->Realm,
                sqlrecord->NASIPAddress,
                sqlrecord->NASPortId,
                sqlrecord->NASPortType,
                sqlrecord->AcctTimeStamp,
                sqlrecord->AcctAuthentic,
                sqlrecord->ConnectInfo,
                sqlrecord->CalledStationId,
                sqlrecord->CallingStationId,
                sqlrecord->ServiceType,
                sqlrecord->FramedProtocol,
                sqlrecord->FramedIPAddress,
                sqlrecord->AcctDelayTime
                );                  

       	        if (sql_query(sql->AcctSock, (const char *) querystr) < 0)
	   	  log(L_ERR, "Acct: Couldn't insert SQL accounting START record - %s", sql_error(sql->AcctSock));
             }

           /* Got stop record */
           } else {

             sprintf(querystr, "SELECT RadAcctId FROM %s WHERE AcctSessionId='%s' AND NASIPAddress='%s' AND UserName='%s'", sql->config.sql_acct_table, sqlrecord->AcctSessionId, sqlrecord->NASIPAddress, sqlrecord->UserName);
              sql_query(sql->AcctSock, querystr);
              if (!(result = sql_store_result(sql->AcctSock)) && sql_num_fields(sql->AcctSock)) {
                   log(L_ERR,"SQL Error: Cannot get result");
                   log(L_ERR,"SQL error: %s",sql_error(sql->AcctSock));
                    sql_close(sql->AcctSock);
                    sql->AcctSock = NULL;
              } else {
                    num = sql_num_rows(result);
	       	    sql_free_result(result);
              }

             if (num > 0) {

                /* Set stop time on matching record with start time */
 	        snprintf(querystr, 2048, "UPDATE %s SET AcctStopTime = '%s', AcctSessionTime = '%lu', AcctInputOctets = '%u', AcctOutputOctets = '%u', AcctTerminateCause = '%s' WHERE AcctSessionId = '%s' AND UserName = '%s' AND NASIPAddress = '%s'", 
	        sql->config.sql_acct_table,
                sqlrecord->AcctTimeStamp,
                sqlrecord->AcctSessionTime,
                sqlrecord->AcctInputOctets,
                sqlrecord->AcctOutputOctets,
                sqlrecord->AcctTerminateCause,
                sqlrecord->AcctSessionId,
                sqlrecord->UserName,
                sqlrecord->NASIPAddress
                );


       	        if (sql_query(sql->config.AcctSock, (const char *) querystr) < 0)
	           log(L_ERR, "Acct: Couldn't update SQL accounting STOP record - %s", sql_error(sql->AcctSock));

             } else if (num == 0) {

            
                /* Insert record with no start time until matching start record comes */
                snprintf(querystr, 2048, "INSERT INTO %s VALUES (0, '%s', '%s', '%s', '%s', %ld, '%s', 0, '%s', '%lu', '%s', '%s', '%u', '%u', '%s', '%s', '%s', '%s', '%s', '%s', %ld)",
                sql->config.sql_acct_table,
                sqlrecord->AcctSessionId,
                sqlrecord->UserName,
                sqlrecord->Realm,
                sqlrecord->NASIPAddress,
                sqlrecord->NASPortId,
                sqlrecord->NASPortType,
	        sqlrecord->AcctTimeStamp,
		sqlrecord->AcctSessionTime,
                sqlrecord->AcctAuthentic,
                sqlrecord->ConnectInfo,
		sqlrecord->AcctInputOctets,
		sqlrecord->AcctOutputOctets,
                sqlrecord->CalledStationId,
                sqlrecord->CallingStationId,
		sqlrecord->AcctTerminateCause,
                sqlrecord->ServiceType,
                sqlrecord->FramedProtocol,
                sqlrecord->FramedIPAddress,
                sqlrecord->AcctDelayTime
                );                  

       	        if (sql->config.sql_query(sql->AcctSock, (const char *) querystr) < 0)
		   log(L_ERR, "Acct: Couldn't insert SQL accounting STOP record - %s", sql_error(sql->AcctSock));
             }

          }
          if (sqlfile) {
                fputs(querystr, sqlfile);
                fputs(";\n", sqlfile);
                fflush(sqlfile);
                fclose(sqlfile);
          }


    	} else {

	    /*
	     *  The database is down for some reason
             *  So open up the backup file to save records in
	     */

             if((backupfile = fopen(SQLBACKUP, "a")) == (FILE *)NULL) {
                 log(L_ERR, "Acct: Couldn't open file %s", SQLBACKUP);
             } else {
                  /*
                   * Lock the sql backup file, prefer lockf() over flock().
                   */
                   #if defined(F_LOCK) && !defined(BSD)
                       (void)lockf((int)backupfile, (int)F_LOCK, (off_t)SQL_LOCK_LEN);
                   #else
                       (void)flock(backupfile, SQL_LOCK_EX);
                   #endif
                   if(fwrite(sqlrecord, sizeof(SQLREC), 1, backupfile) < 1) {
                       log(L_ERR, "Acct: Couldn't write to file %s", SQLBACKUP);
                   }
                   fclose(backupfile);
              }

	}
        
     return 0;

}


/*************************************************************************
 *
 *	Function: sql_userparse
 *
 *	Purpose: Read entries from the database and fill VALUE_PAIR structures
 *
 *************************************************************************/
int sql_userparse(VALUE_PAIR **first_pair, SQL_ROW row) {

	int x;
	char		*s;
	DICT_ATTR	*attr = NULL;
	DICT_VALUE	*dval;
	VALUE_PAIR	*pair, *pair2, *check;
	struct tm	*tm;
	time_t		timeval;


	if((attr = dict_attrfind(row[2])) == (DICT_ATTR *)NULL) {
#if 1 /* Be quiet. */
		log(L_ERR|L_CONS, "unknown attribute %s", row[2]);
#endif	
		return(-1);
	}                              

	/* If attribute is already there, skip it because we checked usercheck first 
	   and we want user settings to over ride group settings */
	if ((check = pairfind(*first_pair, attr->value)) != NULL)
		return 0;

	if((pair = (VALUE_PAIR *)malloc(sizeof(VALUE_PAIR))) == (VALUE_PAIR *)NULL) {
		log(L_CONS|L_ERR, "sql_userparse: no memory");
		exit(1);
	}
	strcpy(pair->name, attr->name);
	pair->attribute = attr->value;
	pair->type = attr->type;
	pair->operator = PW_OPERATOR_EQUAL;
	switch(pair->type) {

		case PW_TYPE_STRING:
			strcpy(pair->strvalue, row[3]);
			pair->length = strlen(pair->strvalue);
			break;

		case PW_TYPE_INTEGER:
                       /*
                        *      For PW_NAS_PORT_ID, allow a
                        *      port range instead of just a port.
                        */
                        if (attr->value == PW_NAS_PORT_ID) {
                              for(s = row[3]; *s; s++)
                                   if (!isdigit(*s)) break;
                                   if (*s) {
                                       pair->type = PW_TYPE_STRING;
                                       strcpy(pair->strvalue, row[3]);
                                       pair->length = strlen(pair->strvalue);
                                       break;
                                   }
                        }
                        if (isdigit(*row[3])) {
                                   pair->lvalue = atoi(row[3]);
                                   pair->length = 4;
                        }
                        else if((dval = dict_valfind(row[3])) == (DICT_VALUE *)NULL) {
                                   free(pair);
                                   log(L_ERR|L_CONS, "unknown value %s", row[3]);
                                   return(-1);
                        }
                        else {
                                   pair->lvalue = dval->value;
                                   pair->length = 4;
                        }
                        break;

		case PW_TYPE_IPADDR:
			if (pair->attribute != PW_FRAMED_IP_ADDRESS) {
                                   pair->lvalue = get_ipaddr(row[3]);
                                   break;
                        }

                       /*
                        *      We allow a "+" at the end to
                        *      indicate that we should add the
                        *      portno. to the IP address.
                        */
                        x = 0;
                        if (row[3][0]) {
                               for(s = row[3]; s[1]; s++) ;
                                    if (*s == '+') {
                                        *s = 0;
                                        x = 1;
                                    }
                        }
                        pair->lvalue = get_ipaddr(row[3]);
                        pair->length = 4;

                       /*
                        *      Add an extra (hidden) attribute.
                        */
                        if((pair2 = malloc(sizeof(VALUE_PAIR))) == NULL) {
                               log(L_CONS|L_ERR, "no memory");
                               exit(1);
                        }
                        strcpy(pair2->name, "Add-Port-To-IP-Address");
                        pair2->attribute = PW_ADD_PORT_TO_IP_ADDRESS;
                        pair2->type = PW_TYPE_INTEGER;
                        pair2->lvalue = x;
                        pair2->length = 4;
                        pairadd(first_pair, pair2);
                        break;

		case PW_TYPE_DATE:
                        timeval = time(0);
                        tm = localtime(&timeval);
                        user_gettime(row[3], tm);
#ifdef TIMELOCAL
                        pair->lvalue = (UINT4)timelocal(tm);
#else
                        pair->lvalue = (UINT4)mktime(tm);
#endif
                        pair->length = 4;
                        break;

		default:
                        free(pair);
#if 1 /* Yeah yeah */
                        log(L_ERR|L_CONS, "unknown attr. type %d", pair->type);
#endif
                        return(-1);
	}
	pairadd(first_pair, pair);

	return 0;
}



/*************************************************************************
 *
 *	Function: sql_getvpdata
 *
 *	Purpose: Get any group check or reply pairs
 *
 *************************************************************************/
int sql_getvpdata(char *table, VALUE_PAIR **vp, char *user, int mode) {

	char		querystr[256];
	SQL_RES		*result;
	SQL_ROW		row;
	int		rows;

	if (mode == PW_VP_USERDATA)
		sprintf(querystr, "SELECT * FROM %s WHERE UserName = '%s'", table, user);
	else if (mode == PW_VP_GROUPDATA)
		sprintf(querystr, "SELECT %s.* FROM %s, %s WHERE %s.UserName = '%s' AND %s.GroupName = %s.GroupName ORDER BY %s.id", table, table, sql->config.sql_usergroup_table, sql->config.sql_usergroup_table, user, sql->config.sql_usergroup_table, table, table);
	else if (mode == PW_VP_REALMDATA)
		sprintf(querystr, "SELECT %s.* FROM %s, %s WHERE %s.RealmName = '%s' AND %s.GroupName = %s.GroupName ORDER BY %s.id", table, table, sql->config.sql_realmgroup_table, sql->config.sql_realmgroup_table, user, sql->config.sql_realmgroup_table, table, table);
        sql_checksocket("Auth");
	sql_query(sql->AuthSock, querystr);
	if ((result = sql_store_result(sql->AuthSock)) && sql_num_fields(sql->AuthSock)) {
		rows = sql_num_rows(result);
		while ((row = sql_fetch_row(result))) {

			if (sql_userparse(vp, row) != 0) {
		 		log(L_ERR|L_CONS, "Error getting data from SQL database");
				sql_free_result(result);
				return -1;
			}
		}
		sql_free_result(result);
	}

	return rows;

}


static int got_alrm;
static void alrm_handler()
{
	got_alrm = 1;
}

/*************************************************************************
 *
 *	Function: sql_check_ts
 *
 *	Purpose: Checks the terminal server for a spacific login entry
 *
 *************************************************************************/
static int sql_check_ts(SQL_ROW row) {

	int     pid, st, e;
	int     n;
	NAS     *nas;
	char    session_id[12];
	char    *s;
	void    (*handler)(int);

	/*
	 *      Find NAS type.
	 */
	if ((nas = nas_find(ipstr2long(row[3]))) == NULL) {
                log(L_ERR, "Accounting: unknown NAS [%s]", row[3]);
                return -1;
        }

        /*
         *      Fork.
         */
        handler = signal(SIGCHLD, SIG_DFL);
        if ((pid = fork()) < 0) {
                log(L_ERR, "Accounting: fork: %s", strerror(errno));
                signal(SIGCHLD, handler);
                return -1;
        }

        if (pid > 0) {
                /*
                 *      Parent - Wait for checkrad to terminate.
                 *      We timeout in 10 seconds.
                 */
                got_alrm = 0;
                signal(SIGALRM, alrm_handler);
                alarm(10);
                while((e = waitpid(pid, &st, 0)) != pid)
                        if (e < 0 && (errno != EINTR || got_alrm))
                                break;
                alarm(0);
                signal(SIGCHLD, handler);
                if (got_alrm) {
                        kill(pid, SIGTERM);
                        sleep(1);
                        kill(pid, SIGKILL);
                        log(L_ERR, "Check-TS: timeout waiting for checkrad");
                        return 2;
                }
                if (e < 0) {
                        log(L_ERR, "Check-TS: unknown error in waitpid()");
                        return 2;
                }
                return WEXITSTATUS(st);
        }

        /*
         *      Child - exec checklogin with the right parameters.
         */
        for (n = 32; n >= 3; n--)
                close(n);

        sprintf(session_id, "%.8s", row[1]);

        s = CHECKRAD2;
        execl(CHECKRAD2, "checkrad", nas->nastype, row[4], row[5],
                row[2], session_id, NULL);
        if (errno == ENOENT) {
                s = CHECKRAD1;
                execl(CHECKRAD1, "checklogin", nas->nastype, row[4], row[5],
                        row[2], session_id, NULL);
        }
        log(L_ERR, "Check-TS: exec %s: %s", s, strerror(errno));

        /*
         *      Exit - 2 means "some error occured".
         */
        exit(2); 

}


/*************************************************************************
 *
 *	Function: sql_check_multi
 *
 *	Purpose: Check radius accounting for duplicate logins
 *
 *************************************************************************/
int sql_check_multi(char *name, VALUE_PAIR *request, int maxsimul) {

	char		querystr[256];
	VALUE_PAIR	*fra;
	SQL_RES		*result;
	SQL_ROW		row;
	int		count = 0;
	UINT4		ipno = 0;
	int		mpp = 1;

	if (!sql_checksocket("Auth"))
		return 0;
	sprintf(querystr, "SELECT COUNT(*) FROM %s WHERE UserName = '%s' AND AcctStopTime = 0", sql->config.sql_acct_table, name);
	sql_query(sql->AuthSock, querystr);
	if (!(result = sql_store_result(sql->AuthSock)) && sql_num_fields(sql->AuthSock)) {
   		log(L_ERR,"SQL Error: Cannot get result");
   		log(L_ERR,"SQL error: %s",sql_error(sql->AuthSock));
   		sql_close(sql->AuthSock);
  		sql->AuthSock = NULL;
	} else {
		row = sql_fetch_row(result);
		count = atoi(row[0]);
		sql_free_result(result);
	}

	if (count < maxsimul)
		return 0;

	/*
	*      Setup some stuff, like for MPP detection.
	*/
	if ((fra = pairfind(request, PW_FRAMED_IP_ADDRESS)) != NULL)
		ipno = htonl(fra->lvalue);

	count = 0;
	sprintf(querystr, "SELECT * FROM %s WHERE UserName = '%s' AND AcctStopTime = 0", sql->config.sql_acct_table, name);
	sql_query(sql->AuthSock, querystr);
	if (!(result = sql_store_result(sql->AuthSock)) && sql_num_fields(sql->AuthSock)) {
   		log(L_ERR,"SQL Error: Cannot get result");
   		log(L_ERR,"SQL error: %s",sql_error(sql->AuthSock));
   		sql_close(sql->AuthSock);
  		sql->AuthSock = NULL;
	} else {
		while ((row = sql_fetch_row(result))) {
			if (sql_check_ts(row) == 1) {
				count++;

				if (ipno && atoi(row[18]) == ipno)
					mpp = 2;   

			} else {
				/*
				 *	False record - zap it
				 */

				sprintf(querystr, "DELETE FROM %s WHERE RadAcctId = '%s'", sql->config.sql_acct_table, row[0]);
				sql_query(sql->AuthSock, querystr);
				
			}
		}
		sql_free_result(result);
	}

	return (count < maxsimul) ? 0 : mpp; 

}
