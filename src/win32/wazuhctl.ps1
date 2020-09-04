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
[string]${ossecconf} = ${basedir} + "ossec.conf"

${xml_ossecconf} = [xml](Get-Content -Path ${ossecconf})


if (${address}) {    
    # Remove old server configuration
    (${xml_ossecconf}.ossec_config.client.SelectNodes("server")) | ForEach-Object {
        [void]$_.ParentNode.RemoveChild($_)
    }

    # Set new server configuration
    ${address} | ForEach-Object {
        $server_element = ${xml_ossecconf}.CreateElement("server")
        $client = ${xml_ossecconf}.ossec_config.SelectSingleNode("client")
        $client.AppendChild($server_element)
    }
}

$i = 0
${port}.GetType()
(${xml_ossecconf}.ossec_config.client.SelectNodes("server")) | ForEach-Object {
    $address_element = ${xml_ossecconf}.CreateElement("address")
    $port_element = ${xml_ossecconf}.CreateElement("port")
    $protocol_element = ${xml_ossecconf}.CreateElement("protocol")
    
    if (${address}) { 
        [void]$_.AppendChild($address_element)
        [void]$_.AppendChild($port_element)
        [void]$_.AppendChild($protocol_element)
        $_.address = ${address}[$i] 
    }

    if (${port}) { $_.port = ${port} }
    elseif (${address}) { $_.port = "1514" }
    
    if (${protocol}) { $_.protocol = ${protocol} }
    elseif (${address}) { $_.protocol = "tcp" }

    $i=$i+1
}

if (${registration-address} || ${registration-port} || ${token} || ${registration-ca} || ${registration-certificate}
    || ${registration-key} || ${name} || ${group}){
    # Remove old autoenrolment configuration
    (${xml_ossecconf}.ossec_config.client.SelectNodes("enrollment")) | ForEach-Object {
        [void]$_.ParentNode.RemoveChild($_)
    }

    $enrollment_element = ${xml_ossecconf}.CreateElement("enrollment")
    $client = ${xml_ossecconf}.ossec_config.SelectSingleNode("client")
    $client.AppendChild($server_element)
    $enrollment = $client.enrollment

    $enabled = ${xml_ossecconf}.CreateElement("enabled")
    $enrollment.AppendChild($enabled)
    
    if (${registration-address}){
        $element = ${xml_ossecconf}.CreateElement("manager_address")
        $enrollment.AppendChild($element)
    }
    if (${registration-port}){
        $element = ${xml_ossecconf}.CreateElement("port")
        $enrollment.AppendChild($element)
    }
    if (${token}){
        $element = ${xml_ossecconf}.CreateElement("authorization_pass")
        $enrollment.AppendChild($element)
    }
    if (${registration-ca}){
        $element = ${xml_ossecconf}.CreateElement("authorization_pass")
        $enrollment.AppendChild($element)
    }
    if (${registration-certificate}){
        $element = ${xml_ossecconf}.CreateElement("server_ca_path")
        $enrollment.AppendChild($element)
    }
    if (${registration-key}){
        $element = ${xml_ossecconf}.CreateElement("agent_key_path")
        $enrollment.AppendChild($element)
    }
    if (${name}){
        $element = ${xml_ossecconf}.CreateElement("agent_name")
        $enrollment.AppendChild($element)
    }
    if (${group}){
        $element = ${xml_ossecconf}.CreateElement("groups")
        $enrollment.AppendChild($element)
    }
}

if (${keep-alive}){
    $reg_port = ${xml_ossecconf}.CreateElement("port")
    $enrollment.AppendChild($reg_port)
}
if (${reconnection-time}){
    $reg_port = ${xml_ossecconf}.CreateElement("port")
    $enrollment.AppendChild($reg_port)
}

${xml_ossecconf}.Save(${ossecconf})