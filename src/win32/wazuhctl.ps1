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
[string]${ossecconf} = ${basedir} + "\ossec.conf"

${xml_conf} = [xml](Get-Content -Path ${ossecconf})


if (${address}) {    
    # Remove old server configuration
    (${xml_conf}.ossec_config.client.SelectNodes("server")) | ForEach-Object {
        [void]$_.ParentNode.RemoveChild($_)
    }

    # Set new server configuration
    $client = ${xml_conf}.ossec_config.SelectSingleNode("client")
    ${address} | ForEach-Object {
        $server_element = ${xml_conf}.CreateElement("server")
        $client.AppendChild($server_element)
    }
}

$i = 0
(${xml_conf}.ossec_config.client.SelectNodes("server")) | ForEach-Object {
    $address_element = ${xml_conf}.CreateElement("address")
    $port_element = ${xml_conf}.CreateElement("port")
    $protocol_element = ${xml_conf}.CreateElement("protocol")
    $keep_alive_element = ${xml_conf}.CreateElement("notify_time")
    $time_reconnect_element = ${xml_conf}.CreateElement("time-reconnect")
    
    if (${address}) { 
        [void]$_.AppendChild($address_element)
        [void]$_.AppendChild($port_element)
        [void]$_.AppendChild($protocol_element)
        [void]$_.AppendChild($keep_alive_element)
        [void]$_.AppendChild($time_reconnect_element)
        $_.address = ${address}[$i] 
    }

    if (${port}) { $_.port = ${port} }
    elseif (${address}) { $_.port = "1514" }
    
    if (${protocol}) { $_.protocol = ${protocol} }
    elseif (${address}) { $_.protocol = "tcp" }

    if (${protocol}) { $_.protocol = ${protocol} }
    elseif (${address}) { $_.protocol = "tcp" }

    if (${reconnection-time}) { $_."time-reconnect" = ${reconnection-time} }
    elseif (${address}) { $_."time-reconnect" = "60" }

    if (${keep-alive}) { $_.notify_time = ${keep-alive} }
    elseif (${address}) { $_.notify_time = "10" }
    
    $i=$i+1
}

if (${registration-address} -or ${registration-port} -or ${token} -or 
    ${registration-ca} -or ${registration-certificate} -or 
    ${registration-key} -or ${name} -or ${group}){
    # Remove old autoenrolment configuration
    (${xml_conf}.ossec_config.client.SelectNodes("enrollment")) | ForEach-Object {
        [void]$_.ParentNode.RemoveChild($_)
    }

    $enrollment_element = ${xml_conf}.CreateElement("enrollment")
    $client = ${xml_conf}.ossec_config.SelectSingleNode("client")
    $client.AppendChild($enrollment_element)
    $enrollment = $client.SelectSingleNode("enrollment")

    $enabled = ${xml_conf}.CreateElement("enabled")
    $enrollment.AppendChild($enabled)
    $enrollment.enabled = "yes"
    
    if (${registration-address}){
        $element = ${xml_conf}.CreateElement("manager_address")
        $enrollment.AppendChild($element)
        $enrollment.manager_address = ${registration-address}
    }
    if (${registration-port}){
        $element = ${xml_conf}.CreateElement("port")
        $enrollment.AppendChild($element)
        $enrollment.port = ${registration-port}
    }
    if (${token}){
        $element = ${xml_conf}.CreateElement("authorization_pass")
        $enrollment.AppendChild($element)
        $enrollment.authorization_pass = ${token}
    }
    if (${registration-ca}){
        $element = ${xml_conf}.CreateElement("authorization_pass")
        $enrollment.AppendChild($element)
        $enrollment.authorization_pass = ${registration-ca}
    }
    if (${registration-certificate}){
        $element = ${xml_conf}.CreateElement("server_ca_path")
        $enrollment.AppendChild($element)
        $enrollment.server_ca_path = ${registration-certificate}
    }
    if (${registration-key}){
        $element = ${xml_conf}.CreateElement("agent_key_path")
        $enrollment.AppendChild($element)
        $enrollment.agent_key_path = ${registration-key}
    }
    if (${name}){
        $element = ${xml_conf}.CreateElement("agent_name")
        $enrollment.AppendChild($element)
        $enrollment.agent_name = ${rname}
    }
    if (${group}){
        $element = ${xml_conf}.CreateElement("groups")
        $enrollment.AppendChild($element)
        $enrollment.groups = ${group}
    }
}

${xml_conf}.Save(${ossecconf})