description = [[
Performs password guessing against http basic authentication
]]

---
-- @usage
-- nmap --script http-brute -p 80 <host>
--
-- This script uses the unpwdb and brute libraries to perform password
-- guessing. Any successful guesses are stored in the nmap registry, under
-- the nmap.registry.credentials.http key for other scripts to use.
--
-- @output
-- PORT     STATE SERVICE REASON
-- 80/tcp   open  http    syn-ack
-- | http-brute:  
-- |   Accounts
-- |     Patrik Karlsson:secret => Login correct
-- |   Statistics
-- |_    Perfomed 60023 guesses in 467 seconds, average tps: 138
--
-- Summary
-- -------
--   x The Driver class contains the driver implementation used by the brute
--     library
--
-- @args http-brute.path points to the path protected by authentication
-- @args http-brute.hostname sets the host header in case of virtual hosting

--
-- Version 0.1
-- Created 07/30/2010 - v0.1 - created by Patrik Karlsson <patrik@cqure.net>
--

author = "Patrik Karlsson"
license = "Same as Nmap--See http://nmap.org/book/man-legal.html"
categories = {"intrusive", "auth"}

require 'shortport'
require 'http'
require 'brute'

portrule = shortport.port_or_service( {80, 443}, {"http", "https"}, "tcp", "open")

Driver = {
	
	new = function(self, host, port)
		local o = {}
       	setmetatable(o, self)
        self.__index = self
		o.host = nmap.registry.args['http-brute.hostname'] or host
		o.port = port
		o.path = nmap.registry.args['http-brute.path']
		return o
	end,
	
	connect = function( self )
		-- This will cause problems, as ther is no way for us to "reserve"
		-- a socket. We may end up here early with a set of credentials
		-- which won't be guessed until the end, due to socket exhaustion.
		return true
	end,
	
	login = function( self, username, password )
		-- we need to supply the no_cache directive, or else the http library
		-- incorrectly tells us that the authentication was successfull
		local response = http.get( self.host, self.port, self.path, { auth = { username = username, password = password }, no_cache = true })

		-- We should probably do more tests here, 500 error and redirects
		-- should be possible candidates. checking for ~= 401 *should* work to
		-- but gave me a number of false positives last time I tried.
		-- After Davids initial review we decided to change it to not 4xx and
		-- not 5xx. That would roughly equal the following:
		if ( response.status < 400 or response.status > 599 ) then
			if ( not( nmap.registry['credentials'] ) ) then
				nmap.registry['credentials'] = {}
			end
			if ( not( nmap.registry.credentials['http'] ) ) then
				nmap.registry.credentials['http'] = {}
			end
			table.insert( nmap.registry.credentials.http, { username = username, password = password } )
			return true, brute.Account:new( username, password, "OPEN")
		end
		return false, brute.Error:new( "Incorrect password" )
	end,
	
	disconnect = function( self ) 
		return true
	end,
	
	check = function( self )
		local response = http.get( self.host, self.port, self.path )
		
		if ( response.status == 401 ) then
			return true
		end
		return false
	end,
	
}


action = function( host, port )
	local status, result 
	local engine = brute.Engine:new(Driver, host, port )
   	local path = nmap.registry.args['http-brute.path']

	if ( not(path) ) then
		return "  \n  ERROR: No path was specified (see http-brute.path)"
	end
	
	local response = http.get( host, port, path )
	
	if ( response.status ~= 401 ) then
		return ("  \n  Path \"%s\" does not require autentication"):format(path)
	end
	

	-- there's a bug in http.lua that does not allow it to be called by
	-- multiple threads
	engine:setMaxThreads(1)
	status, result = engine:start()
	
	return result
end