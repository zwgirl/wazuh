<#
.Description
This tool will download, install and configure the selected version of Wazuh-agent.
If no version is selected it will install the latest one.
.Synopsis
Tool to Install and configure Windows Wazuh Agent

.PARAMETER version
 Wazuh agent package version
.PARAMETER revision
 Wazuh agent package revision
.PARAMETER address
 Wazuh manager address
.PARAMETER port
 Wazuh manager port
.PARAMETER protocol
 Wazuh manager protocol
.PARAMETER registration-address
 Wazuh registration address
.PARAMETER registration-port
 Wazuh registration port
.PARAMETER token
 Wazuh registration password
.PARAMETER keep-alive
 Wazuh agent keep alive time
.PARAMETER reconnection-time
 Wazuh agent reconnection time
.PARAMETER registration-ca
 Certification Authority (CA) path
.PARAMETER registration-certificate 
 Registration certificate path
.PARAMETER registration-key
 Registration key path
.PARAMETER name
 Wazuh agent name
.PARAMETER group
 Wazuh agent group

 .example
 Set Wazuh Manager address and register
./wazuhctl.ps1 enroll -address [MANAGER_ADDRESS]

 .example
 Set Wazuh Manager address and register to a different manager
./wazuhctl.ps1 enroll -address [MANAGER_ADDRESS] -registration-address [REGISTRATION_ADDRESS]

 .example
 Set Wazuh Manager address and register using password
./wazuhctl.ps1 enroll -address [MANAGER_ADDRESS] -token [PASSWORD]
#> 

param (
   [version]${version},
   [string]${revision},
   [string[]]${address},
   [string]${port},
   [string]${protocol},
   [string]${registration-address},
   [string]${registration-port},
   [string]${token},
   [string]${keep-alive},
   [string]${reconnection-time},
   [string]${registration-ca},
   [string]${registration-certificate},
   [string]${registration-key},
   [string]${name},
   [string]${group}

)

$params = @{}

if(!$version){
   [version]${version} = "4.0.0"
}

if(!$revision){
   [string]${version} = "1"
}

if($address){
   $params.Add("-address","$address")
}
if($address){
   $params.Add("-port","$port")
}
if($address){
   $params.Add("-protocol","$protocol")
}
if($address){
   $params.Add("-registration-address","${registration-address}")
}
if($address){
   $params.Add("-registration-port","${registration-port}")
}
if($address){
   $params.Add("-token","$token")
}
if($address){
   $params.Add("-keep-alive","${keep-alive}")
}
if($address){
   $params.Add("-reconnection-time","${reconnection-time}")
}
if($address){
   $params.Add("-registration-ca","${registration-ca}")
}
if($address){
   $params.Add("-registration-certificate","${registration-certificate}")
}
if($address){
   $params.Add("-registration-key","${registration-key}")
}

$url = "https://packages.wazuh.com/$($v.major).x/windows/wazuh-agent-$($v.ToString())-${revision}.msi"
$output = "wazuh-agent-${versionStr}-${revision}.msi"

[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
Invoke-WebRequest -Uri $url -OutFile $output

powershell.exe ".\$output /q"

Start-Sleep -s 1
&'C:\Program Files (x86)\ossec-agent\wazuhctl.ps1' -enroll @params