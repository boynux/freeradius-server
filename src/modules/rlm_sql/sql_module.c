/*
 * sql_module.c	- MySQL routines for FreeRADIUS SQL module 
 *
 * Mike Machado <mike@innercite.com>
 */

#include	"rlm_sql.h"
#include	"sql_module.h"


/*************************************************************************
 *
 *	Function: sql_connect
 *
 *	Purpose: Connect to the sql server
 *
 *************************************************************************/
int sql_connect(void) {

	MYSQL MyAuthConn;
	MYSQL MyAcctConn;

        /* Connect to the database server */
        mysql_init(&MyAuthConn);
        if (!(sql->AuthSock = mysql_real_connect(&MyAuthConn, sql->config.sql_server, sql->config.sql_login, sql->config.sql_password, sql->config.sql_db, 0, NULL, 0))) {
             log(L_ERR, "Init: Couldn't connect authentication socket to MySQL server on %s as %s", sql->config.sql_server, sql->config.sql_login);
             sql->AuthSock = NULL;
        }
        mysql_init(&MyAcctConn);
        if (!(sql->AcctSock = mysql_real_connect(&MyAcctConn, sql->config.sql_server, sql->config.sql_login, sql->config.sql_password, sql->config.sql_db, 0, NULL, 0))) {
             log(L_ERR, "Init: Couldn't connect accounting socket to MySQL server on %s as %s", sql->config.sql_server, sql->config.sql_login);
             sql->AcctSock = NULL;
        }
           
       return 0;
}

 

/*************************************************************************
 *
 *	Function: sql_checksocket
 *
 *	Purpose: Make sure our database connection is up
 *
 *************************************************************************/
int sql_checksocket(const char *facility) {

	if ((strncmp(facility, "Auth", 4) == 0)) {
		if (sql->AuthSock == NULL) {

			MYSQL MyAuthConn;
			if (sql->config.sql_keepopen)
				log(L_ERR, "%s: Keepopen set but had to reconnect to MySQL", facility);
			/* Connect to the database server */
			mysql_init(&MyAuthConn);
			if (!(sql->AuthSock = mysql_real_connect(&MyAuthConn, sql->config.sql_server, sql->config.sql_login, sql->config.sql_password, sql->config.sql_db, 0, NULL, 0))) {
				log(L_ERR, "Auth: Couldn't connect authentication socket to MySQL server on %s as %s", sql->config.sql_server, sql->config.sql_login);
				sql->AuthSock = NULL;
				return 0;
			}
		}

	} else {
		if (MyAcctSock == NULL) {
			MYSQL MyAcctConn;
			if (mysql_keepopen)
				log(L_ERR, "%s: Keepopen set but had to reconnect to MySQL", facility);
			/* Connect to the database server */
			mysql_init(&MyAcctConn);
			if (!(sql->AcctSock = mysql_real_connect(&MyAcctConn, sql->config.sql_server, sql->config.sql_login, sql->config.sql_password, sql->config.sql_db, 0, NULL, 0))) {
				log(L_ERR, "Acct: Couldn't connect accounting socket to MySQL server on %s as %s", sql->config.sql_server, sql->config.sql_login);
				sql->AcctSock = NULL;
				return 0;
			}
		}

	}

	return 1;

}


/*************************************************************************
 *
 *	Function: sql_query
 *
 *	Purpose: Issue a query to the database
 *
 *************************************************************************/
void sql_query(SQLSOCK *socket, char *querystr) {

 if (sql->config.sqltrace)
	DEBUG(querystr);
 mysql_query(socket, querystr);

}



/*************************************************************************
 *
 *	Function: sql_store_result
 *
 *	Purpose: database specific store_result function. Returns a result
 *               set for the query.
 *
 *************************************************************************/
SQL_RES *sql_store_result(SQLSOCK *socket) {

	SQL_RES	*result;

	if (!(result = mysql_store_result(socket)) {
		log(L_ERR,"MYSQL Error: Cannot get result");
		log(L_ERR,"MYSQL error: %s",mysql_error(socket));
		sql_close(socket);
	}
	return result;

}


/*************************************************************************
 *
 *	Function: sql_num_fields
 *
 *	Purpose: database specific num_fields function. Returns number
 *               of columns from query
 *
 *************************************************************************/
int sql_num_fields(SQLSOCK *socket) {

	int	num = 0;
	if (!(num = mysql_num_fields(socket)) {
		log(L_ERR,"MYSQL Error: Cannot get result");
		log(L_ERR,"MYSQL error: %s",mysql_error(socket));
		sql_close(socket);
	}
	return num;
}


/*************************************************************************
 *
 *	Function: sql_num_rows
 *
 *	Purpose: database specific num_rows. Returns number of rows in
 *               query
 *
 *************************************************************************/
int sql_num_rows(SQL_RES *result) {

    return mysql_num_rows(result);

}


/*************************************************************************
 *
 *	Function: sql_fetch_row
 *
 *	Purpose: database specific fetch_row. Returns a SQL_RES struct
 *               with all the data for the query
 *
 *************************************************************************/
SQL_ROW sql_fetch_row(SQL_RES *result) {

   return mysql_fetch_row(result);

}



/*************************************************************************
 *
 *	Function: sql_free_result
 *
 *	Purpose: database specific free_result. Frees memory allocated
 *               for a result set
 *
 *************************************************************************/
int sql_free_result(SQL_RES *result) {

   return mysql_free_result(result);

}



/*************************************************************************
 *
 *	Function: sql_error
 *
 *	Purpose: database specific error. Returns error associated with
 *               connection
 *
 *************************************************************************/
char *sql_error(SQLSOCK *socket) {

  return (mysql_error(socket));

}


/*************************************************************************
 *
 *	Function: sql_close
 *
 *	Purpose: database specific close. Closes an open database
 *               connection
 *
 *************************************************************************/
int sql_close(SQLSOCK *socket) {

   return mysql_close(socket);

}


