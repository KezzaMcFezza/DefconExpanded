#!/usr/bin/python

# compatibility wrapper for sqlite

interfaceVersion = 2

try:
    from pysqlite2 import dbapi2 as sqlite
except:
    interfaceVersion = 1
    import sqlite

DatabaseError = sqlite.DatabaseError
OperationalError = sqlite.OperationalError

# wrap connections
def connect( filename, timeout=60.0 ):
    class ConnectionWrapper:
        def __init__( self, connection ):
            self.connection = connection
        def commit( self ):
            self.connection.commit()

        # wrap cursors
        def cursor( self ):
            class CursorWrapper:
                def __init__( self, cursor ):
                    self.cursor = cursor
                def execute( self, SQL, parameters=[] ):
                    global interfaceVersion
                    if interfaceVersion == 2:
                        return self.cursor.execute( SQL, parameters )
                    else:
                        return self.cursor.execute( SQL.replace('?', '%s'), parameters )

                def fetchone( self ):
                    return self.cursor.fetchone()
                def fetchall( self ):
                    return self.cursor.fetchall()
                
            return CursorWrapper( self.connection.cursor() )

    return ConnectionWrapper( sqlite.connect( filename, timeout=timeout ) )
