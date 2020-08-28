param (
    [string[]]${address} = $env:address,
    [string]${port} = $env:port,
    [string]${protocol} = $env:address,
    [string]${registration-address} = $env:registration_address,
    [string]${registration-port} = $env:registration_port,
    [string]${token} = $env:token,
    [string]${keep-alive} = $env:keep_alive,
    [string]${reconnection-time} = $env:reconnection_time,
    [string]${registration-ca} = $env:registration_ca,
    [string]${registration-certificate} = $env:registration_certificate,
    [string]${registration-key} = $env:registration_key,
    [string]${name} = $env:name,
    [string]${group} = $env:group,
    [string]${help} = $env:help
)

[string]${basedir} = (split-path -parent $MyInvocation.MyCommand.Definition)
[string]${ossecconf} = (Join-Path -Path ${basedir} -ChildPath "ossec.conf")


function Edit-Tag {
    param (
        $Tag
        $Value
    )
    ${ossecconf} -match "<$Tag>.*</$Tag>"-replace "<$Tag>$Value</$Tag>"
}

