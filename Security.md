# Security
Disqueue offers two methods to imrpove the security of your server - SSL and
Authentication.

## SSL
SSL is enabled in the configuration file, per server. On a server that you
wish to enable SSL for you should set the `security` property to an object
containing the key `certificate` and `privatekey`, both of which point to the
PEM encoded certificate/private key to use for SSL. 
```javascript
{
  "servers": [
    {
      "hostname": "0.0.0.0",
      "port": 3683,
      "security": {
        "certificate": "/etc/disqueue/certificate.pem",
        "privatekey": "/etc/disqueue/private.key"
      }
    }
  ]
}
```

## Authentication
Authentication is done using HTTP Basic authentication, and configured in the
server file. You can use the same authentication method for all servers, or set
an individual authentication method per server. To begin, you must create an
authentication method instance - in the root object create a key called
`authentication` which holds each authentication method. The authentication
method is named using the key of the property, and should have a value which
contains the properties `type` and `file`. `type` should be one of the supported
types and `file` the location of the file storing the passwords, encoded for the
type. The authentication method is added to a server using the `authentication`
property on the server and the name of the method instance.

```javascript
{
  "servers": [
    {
      "hostname": "0.0.0.0",
      "port": 3682,
      "authentication": "default"
    }
  ],
  "authentication": {
    "default": {
      "type": "plaintext",
      "file": "/etc/disqueue/passwords"
    }
  }
}
```

### Supported Types
* plaintext

#### Plaintext password file
Plaintext password file is the most simple authentication method. The file has
one line per username, and the username and password are separated by a colon.
The password is in modular crypt format, with the following supported algorithms:
  * `plain`: `$plain$your_password`


example: `/etc/disqueue/passwords`
```
username:$plain$password
```
