<?php
/**
 * Copyright (C) 2013-2014 OpenMediaVault Plugin Developers
 * Copyright (C) 2009-2013 Volker Theile <volker.theile@openmediavault.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
require_once "openmediavault/config.inc";
require_once "openmediavault/error.inc";
require_once "openmediavault/notify.inc";
require_once "openmediavault/object.inc";
require_once "openmediavault/rpcservice.inc";
require_once "openmediavault/system.inc";
require_once "openmediavault/util.inc";

class OMVRpcServiceBackup extends OMVRpcServiceAbstract
{

    private static $commands = array("backup","only","rescue");

    public function getName()
    {
        return 'Backup';
    }

    private function getPluginName()
    {
        return strtolower( $this->getName() );
    }

    private function getXPath()
    {
        $path = sprintf( "//system/%s", $this->getPluginName() );
        return $path;
    }

    private function getXPathExtundelete()
    {
        $path = sprintf( "%s/extundelete", $this->getXPath() );
        return $path;
    }

    private function getXPathPhotorec()
    {
        $path = sprintf( "%s/photorec", $this->getXPath() );
        return $path;
    }

    private function getXPathPartedmagic()
    {
        $path = sprintf( "%s/partedmagic", $this->getXPath() );
        return $path;
    }

    private function getXPathRear()
    {
        $path = sprintf( "%s/rear", $this->getXPath() );
        return $path;
    }

    private function getEventMessagePath()
    {
        $path = sprintf( "org.openmediavault.system.%s", $this->getPluginName() );
        return $path;
    }

    private function getEventMessagePathExtundelete()
    {
        $path = sprintf( "%s.extundelete", $this->getEventMessagePath() );
        return $path;
    }

    private function getEventMessagePathPhotorec()
    {
        $path = sprintf( "%s.photorec", $this->getEventMessagePath() );
        return $path;
    }

    private function getEventMessagePathRear()
    {
        $path = sprintf( "%s.rear", $this->getEventMessagePath() );
        return $path;
    }

    private function getXPathSystemShare()
    {
        return "//system/shares/sharedfolder";
    }

    private function getXPathMntent()
    {
        return "//system/fstab/mntent";
    }

    private function getExtundeleteBin()
    {
        return "/usr/bin/extundelete";
    }

    private function getPhotorecBin()
    {
        return "/usr/bin/photorec";
    }

    private function getRearBin()
    {
        return "/usr/sbin/rear";
    }

    public function initialize()
    {
        $this->registerMethod('getSettings');
        $this->registerMethod('setSettings');
        $this->registerMethod('doBackup');

        $this->registerMethod('doInstallISO');
        $this->registerMethod('doRebootISO');
        $this->registerMethod('doRebootOMV');

        $this->registerMethod('getPartedmagic');
        $this->registerMethod('setPartedmagic');

        $this->registerMethod('getCandidates');
        $this->registerMethod('getPhotorec');
        $this->registerMethod('setPhotorec');
        $this->registerMethod('getDrives');
        $this->registerMethod('doRecover');
        $this->registerMethod('doSilent');

        $this->registerMethod('getExtundelete');
        $this->registerMethod('setExtundelete');
        $this->registerMethod('doRecoverExt');
        $this->registerMethod('doSilentExt');

        $this->registerMethod('getRear');
        $this->registerMethod('setRear');
        $this->registerMethod('doRearBackup');
        $this->registerMethod('doDump');
    }

    public function getSettings($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Get configuration object
        $object = $xmlConfig->get( $this->getXPath() );
        if (is_null($object))
            throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED, $this->getXPath() );

        // Remove useless fields from the result object.
        unset($object['extundelete']);
        unset($object['photorec']);
        unset($object['partedmagic']);
        unset($object['rear']);

        return $object;
    }

    public function setSettings($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, '{
            "type"       : "object",
            "properties" : {
                "mntentref" : { "type" : "string" },
                "path"      : { "type" : "string" }
            }
        }');

        $xPath = sprintf("%s[uuid='%s']", $this->getXPathMntent(), $params['mntentref']);
        $deviceDir = $xmlConfig->get($xPath);
        $deviceDir = $deviceDir['dir'];

        $backupPath = "{$deviceDir}/omvbackup";
        $params['path'] = $backupPath;

        // Get the existing configuration object.
        $oldObject = $xmlConfig->get( $this->getXPath() );
        if (is_null($oldObject))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXPath()
            );
        }

        // Prepare configuration data
        $object = array(
            "mntentref"   => $params['mntentref'],
            "path"        => $params['path'],
            "photorec"    => isset($oldObject['photorec']) ? $oldObject['photorec'] : array(),
            "partedmagic" => isset($oldObject['partedmagic']) ? $oldObject['partedmagic'] : array(),
            "extundelete" => isset($oldObject['extundelete']) ? $oldObject['extundelete'] : array(),
            "rear"        => isset($oldObject['rear']) ? $oldObject['rear'] : array()
        );

        // Set configuration object
        if (false === $xmlConfig->replace($this->getXPath(), $object))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED,
                $this->getXPath()
            );
        }

        // Notify configuration changes
        $dispatcher = &OMVNotifyDispatcher::getInstance();
        $dispatcher->notify(
            OMV_NOTIFY_MODIFY,
            $this->getEventMessagePath(),
            $object
        );
    }

    public function doBackup($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Create a background process.
        $bgStatusFilename = $this->createBgProcStatus();
        $pid = $this->fork();
        if ($pid > 0) { // Parent process.
            $this->initializeBgProcStatus($bgStatusFilename, $pid);
            return $bgStatusFilename;
        }

        // Child process.
        try {
            $bgOutputFilename = $this->createBgProcOutput();
            $this->updateBgProcStatus($bgStatusFilename, "outputfilename", $bgOutputFilename);

            $cmd = sprintf("omv-mkconf %s backup 2>&1", $this->getPluginName() );
            $this->exec($cmd, $output, $bgOutputFilename);
            $this->finalizeBgProcStatus($bgStatusFilename, $output);
            exit(0);
        } catch(Exception $e) {
            $this->finalizeBgProcStatus($bgStatusFilename, "", $e);
            exit(1);
        }
    }

    public function doInstallISO($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, '{
            "type"       : "object",
            "properties" : {
                "command" : { "type" : "string" }
            }
        }');

        $commands = array("cz", "sys", "pm", "gp");
        if (!in_array($params['command'], $commands))
        {
            throw new OMVException(
                OMVErrorMsg::E_MISC_OPERATION_DENIED,
                sprinf("Unknown command - %s", $params['command'])
            );
        }

        // Create a background process.
        $bgStatusFilename = $this->createBgProcStatus();
        $pid = $this->fork();
        if ($pid > 0) { // Parent process.
            $this->initializeBgProcStatus($bgStatusFilename, $pid);
            return $bgStatusFilename;
        }

        // Child process.
        try {
            $bgOutputFilename = $this->createBgProcOutput();
            $this->updateBgProcStatus($bgStatusFilename, "outputfilename", $bgOutputFilename);

            $cmd = sprintf("omv-mkconf %s install%s 2>&1", $this->getPluginName(), $params['command']);
            $this->exec($cmd, $output, $bgOutputFilename);
            $this->finalizeBgProcStatus($bgStatusFilename, $output);
            exit(0);
        } catch(Exception $e) {
            $this->finalizeBgProcStatus($bgStatusFilename, "", $e);
            exit(1);
        }
    }

    public function doRebootISO($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, '{
            "type"       : "object",
            "properties" : {
                "command" : { "type" : "string" }
            }
        }');

        $commands = array("clonezilla", "sysresccd", "partedmagic", "gpartedlive");
        if (!in_array($params['command'], $commands))
        {
            throw new OMVException(
                OMVErrorMsg::E_MISC_OPERATION_DENIED,
                sprinf("Unknown command - %s", $params['command'])
            );
        }

        // Create a background process.
        $bgStatusFilename = $this->createBgProcStatus();
        $pid = $this->fork();
        if ($pid > 0) { // Parent process.
            $this->initializeBgProcStatus($bgStatusFilename, $pid);
            return $bgStatusFilename;
        }

        // Child process.
        try {
            $bgOutputFilename = $this->createBgProcOutput();
            $this->updateBgProcStatus($bgStatusFilename, "outputfilename", $bgOutputFilename);

            $cmd = sprintf("omv-mkconf %s %s 2>&1", $this->getPluginName(), $params['command'] );
            $this->exec($cmd, $output, $bgOutputFilename);
            $this->finalizeBgProcStatus($bgStatusFilename, $output);
            exit(0);
        } catch(Exception $e) {
            $this->finalizeBgProcStatus($bgStatusFilename, "", $e);
            exit(1);
        }
    }

    public function getPartedmagic($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Get configuration object
        $object = $xmlConfig->get( $this->getXPathPartedmagic() );
        if (is_null($object))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXPathPartedmagic()
            );
        }

        return $object;
    }

    public function setPartedmagic($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, '{
            "type"       : "object",
            "properties" : {
                "pmref" : { "type" : "string" }
            }
        }');

        // Prepare configuration data
        $object = array(
            "pmref" => $params['pmref']
        );

        // Set configuration object
        if (false === $xmlConfig->replace($this->getXPathPartedmagic(), $object))
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED,
                $this->getXPathPartedmagic()
            );
    }

    public function doRebootOMV($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Create a background process.
        $bgStatusFilename = $this->createBgProcStatus();
        $pid = $this->fork();
        if ($pid > 0) { // Parent process.
            $this->initializeBgProcStatus($bgStatusFilename, $pid);
            return $bgStatusFilename;
        }

        // Child process.
        try {
            $bgOutputFilename = $this->createBgProcOutput();
            $this->updateBgProcStatus($bgStatusFilename, "outputfilename", $bgOutputFilename);

            $cmd = sprintf("omv-mkconf %s omv 2>&1", $this->getPluginName() );
            $this->exec($cmd, $output, $bgOutputFilename);
            $this->finalizeBgProcStatus($bgStatusFilename, $output);
            exit(0);
        } catch(Exception $e) {
            $this->finalizeBgProcStatus($bgStatusFilename, "", $e);
            exit(1);
        }
    }

    public function getCandidates($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Get a list of mount points, except bind mounts.
        $xPath = sprintf("%s[not(contains(opts,'bind'))]", $this->getXPathMntent() );
        $objects = $xmlConfig->getList($xPath);
        if (is_null($objects))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $xPath
            );
        }

        // Get the filesystem details for each mount point.
        $result = array();
        foreach($objects as $objectk => $objectv)
        {
            $fs = new OMVFilesystem($objectv['fsname']);
            if (!$fs->exists())
                continue;

            // Skip the filesystems that are not mounted at the moment.
            if (FALSE === $fs->isMounted())
                continue;

            // Get some filesystem details.
            $devicefile = $fs->getDeviceFile();
            $label = $fs->getLabel();

            // Prepare result object.
            $object = array(
                "uuid"        => $objectv['uuid'],
                "devicefile"  => $devicefile,
                "description" => !empty($label) ? $label : $devicefile
            );

            // Get some more filesystem details if possible.
            if (FALSE !== ($fsStats = $fs->getStatistics()))
            {
                $object['description'] = sprintf(
                    gettext("%s (%s available)"),
                    $object['description'],
                    binary_format($fsStats['available'])
                );
            }

            $result[] = $object;
        }

        return $result;
    }

    public function getPhotorec($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Get configuration object
        $object = $xmlConfig->get( $this->getXPathPhotorec() );
        if (is_null($object))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXPathPhotorec()
            );
        }

        // Modify the result object.
        $options = array("freespace", "everything", "bmp", "bz2", "doc", "gz", "mkv", "mov",
            "mp3", "mpg", "pdf", "png", "raw", "tif", "tx", "txt", "zip");

        foreach ($options as $value)
        {
            $object[$value] = array_boolval($object, $value);
        }

        return $object;
    }

    public function setPhotorec($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, '{
            "type"       : "object",
            "properties" : {
                "source"      : { "type" : "string" },
                "destination" : { "type" : "string" },
                "freespace"   : { "type" : "boolean" },
                "everything"  : { "type" : "boolean" },
                "bmp"         : { "type" : "boolean", "optional" : true },
                "bz2"         : { "type" : "boolean", "optional" : true },
                "doc"         : { "type" : "boolean", "optional" : true },
                "gz"          : { "type" : "boolean", "optional" : true },
                "mkv"         : { "type" : "boolean", "optional" : true },
                "mov"         : { "type" : "boolean", "optional" : true },
                "mp3"         : { "type" : "boolean", "optional" : true },
                "mpg"         : { "type" : "boolean", "optional" : true },
                "pdf"         : { "type" : "boolean", "optional" : true },
                "png"         : { "type" : "boolean", "optional" : true },
                "raw"         : { "type" : "boolean", "optional" : true },
                "tif"         : { "type" : "boolean", "optional" : true },
                "tx"          : { "type" : "boolean", "optional" : true },
                "txt"         : { "type" : "boolean", "optional" : true },
                "zip"         : { "type" : "boolean", "optional" : true }
            }
        }');

        // Check to see if shared folder is on source drive.
        // Get shared folder configuration object.
        $xPath = sprintf("%s[uuid='%s']", $this->getXPathSystemShare(), $params['destination']);
        $sfObject = $xmlConfig->get($xPath);
        if (is_null($sfObject))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $xPath
            );
        }

        // Get the mount point configuration object to build the absolute shared folder path.
        $xPath = sprintf("%s[uuid='%s']", $this->getXPathMntent(), $sfObject['mntentref']);
        $meObject = $xmlConfig->get($xPath);
        if (is_null($meObject))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $xPath
            );
        }

        $cmd = sprintf("mount | grep %s | grep %s", $params['source'], $meObject['dir']);
        $this->exec($cmd, $output);
        if ( count($output) > 0 )
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_OBJECT_UNIQUENESS,
                gettext(
                    sprintf("Shared folder cannot be located on the source drive - %s", $params['source'])
                )
            );
        }

        // Get the existing configuration object.
        $oldObject = $xmlConfig->get( $this->getXPathPhotorec() );
        if (is_null($oldObject))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXPathPhotorec()
            );
        }

        // Prepare configuration data
        $object = array(
            "source"      => $params['source'],
            "destination" => $params['destination'],
            "freespace"   => array_boolval($params, 'freespace'),
            "everything"  => array_boolval($params, 'everything'),
            "bmp"         => array_boolval($params, 'bmp'),
            "bz2"         => array_boolval($params, 'bz2'),
            "doc"         => array_boolval($params, 'doc'),
            "gz"          => array_boolval($params, 'gz'),
            "mkv"         => array_boolval($params, 'mkv'),
            "mov"         => array_boolval($params, 'mov'),
            "mp3"         => array_boolval($params, 'mp3'),
            "mpg"         => array_boolval($params, 'mpg'),
            "pdf"         => array_boolval($params, 'pdf'),
            "png"         => array_boolval($params, 'png'),
            "raw"         => array_boolval($params, 'raw'),
            "tif"         => array_boolval($params, 'tif'),
            "tx"          => array_boolval($params, 'tx'),
            "txt"         => array_boolval($params, 'txt'),
            "zip"         => array_boolval($params, 'zip')
        );

        // Set configuration object
        if (false === $xmlConfig->replace($this->getXPathPhotorec(), $object))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED,
                $this->getXPathPhotorec()
            );
        }

        // Notify configuration changes
        $dispatcher = &OMVNotifyDispatcher::getInstance();
        $dispatcher->notify(OMV_NOTIFY_MODIFY, $this->getEventMessagePathPhotorec(), $object);
    }

    public function getDrives($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, '{
            "type"       : "object",
            "properties" : {
                "extonly" : { "type" : "boolean", "optional" : true }
            }
        }');

        $extonly = boolval($params['extonly']);
        if ($extonly === true)
            $cmd = "blkid -o list | grep ext[3-4] | grep 'not mounted'";
        else
            $cmd = 'lsblk -dno name,size 2>&1';

        OMVUtil::exec($cmd, $output, $result);

        if ($result !== 0)
            return false;

        $drives = array();
        foreach($output as $row)
        {
            $info = preg_split('/\s+/', $row);

            if ($extonly === true)
            {
                $object = array(
                    "drive"   => $info[0],
                    "display" => $info[0]
                );
            }
            else
            {
                $object = array(
                    "drive"   => sprintf("/dev/%s", $info[0]),
                    "display" => sprintf("/dev/%s - %s", $info[0], $info[1])
                );
            }
            $drives[] = $object;
        }

        return $drives;
    }

    public function doRecover($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Get the configuration object.
        $object = $xmlConfig->get( $this->getXPathPhotorec() );
        if (is_null($object))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXPathPhotorec()
            );
        }

        $options = array("bmp", "bz2", "doc", "gz", "mkv", "mov", "mp3", "mpg",
            "pdf", "png", "raw", "tif", "tx", "txt", "zip");

        // Get options
        if ( boolval($object['everything']) )
        {
            $cmd2 = "everything,enable";
        }
        else
        {
            $cmd2 = "everything,disable";

            foreach ($options as $value)
            {
                if ( boolval($object[$value]) )
                    $cmd2 .= sprintf(",%s,enable", $value);
            }
        }
        if ( boolval($object['freespace']) )
            $cmd2 .= ",freespace";

        // Get paths
        $sourcePath = $object['source'];
        $destPath = OMVRpc::exec("ShareMgmt", "getPath", array("uuid" => $object['destination']), $context);

        // Create a background process.
        $bgStatusFilename = $this->createBgProcStatus();
        $pid = $this->fork();
        if ($pid > 0) { // Parent process.
            $this->initializeBgProcStatus($bgStatusFilename, $pid);
            return $bgStatusFilename;
        }

        // Child process.
        try {
            $bgOutputFilename = $this->createBgProcOutput();
            $this->updateBgProcStatus($bgStatusFilename, "outputfilename", $bgOutputFilename);

            $cmd = sprintf("%s /log /logname \"%sphotorec.log\" /d %s /cmd %s fileopt,%s,search 2>&1",
                $this->getPhotorecBin(), $destPath, $destPath, $sourcePath, $cmd2
            );

            $this->exec($cmd, $output, $bgOutputFilename);
            $this->finalizeBgProcStatus($bgStatusFilename, $output);
            exit(0);
        } catch(Exception $e) {
            $this->finalizeBgProcStatus($bgStatusFilename, "", $e);
            exit(1);
        }
    }

    function doSilent($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Get the configuration object.
        $object = $xmlConfig->get( $this->getXPathPhotorec() );
        if (is_null($object))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXPathPhotorec()
            );
        }

        $options = array("bmp", "bz2", "doc", "gz", "mkv", "mov", "mp3", "mpg",
            "pdf", "png", "raw", "tif", "tx", "txt", "zip");

        // Get options
        if ( boolval($object['everything']) )
        {
            $cmd2 = "everything,enable";
        }
        else
        {
            $cmd2 = "everything,disable";

            foreach ($options as $value)
            {
                if ( boolval($object[$value]) )
                    $cmd2 .= sprintf(",%s,enable", $value);
            }
        }
        if ( boolval($object['freespace']) )
            $cmd2 .= ",freespace";

        // Get paths
        $sourcePath = $object['source'];
        $destPath = OMVRpc::exec("ShareMgmt", "getPath", array("uuid" => $object['destination']), $context);

        $cmd = sprintf("%s /log /logname \"%sphotorec.log\" /d %s /cmd %s fileopt,%s,search 2>&1",
            $this->getPhotorecBin(), $destPath, $destPath, $sourcePath, $cmd2
        );
        //$cmd = "/usr/bin/photorec /log /logname \"{$destPath}photorec.log\" /d {$destPath} /cmd {$sourcePath} fileopt,{$cmd2},search 2>&1";

        if (0 !== $this->exec($cmd, $output))
        {
            throw new OMVException(
                OMVErrorMsg::E_EXEC_FAILED,
                $cmd,
                implode("\n", $output)
            );
        }
    }

    public function getExtundelete($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Get configuration object
        $object = $xmlConfig->get( $this->getXPathExtundelete() );
        if (is_null($object))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXPathExtundelete()
            );
        }

        return $object;
    }

    public function setExtundelete($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, '{
            "type"       : "object",
            "properties" : {
                "source"      : { "type" : "string" },
                "destination" : { "type" : "string" }
            }
        }');

        // Check to see if shared folder is on source drive.
        // Get shared folder configuration object.
        $xPath = sprintf("%s[uuid='%s']", $this->getXPathSystemShare(), $params['destination']);
        $sfObject = $xmlConfig->get($xPath);
        if (is_null($sfObject))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $xPath
            );
        }

        // Get the mount point configuration object to build the absolute shared folder path.
        $xPath = sprintf("%s[uuid='%s']", $this->getXPathMntent(), $sfObject['mntentref']);
        $meObject = $xmlConfig->get($xPath);
        if (is_null($meObject))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $xPath
            );
        }

        $cmd = sprintf("mount | grep %s | grep %s", $params['source'], $meObject['dir']);
        $this->exec($cmd, $output);
        if ( count($output) > 0 )
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_OBJECT_UNIQUENESS,
                gettext(sprintf("Shared folder cannot be located on the source drive - %s", $params['source']))
            );
        }

        // Get the existing configuration object.
        $oldObject = $xmlConfig->get( $this->getXPathExtundelete() );
        if (is_null($oldObject))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXPathExtundelete()
            );
        }

        // Prepare configuration data
        $object = array(
            "source"      => $params['source'],
            "destination" => $params['destination']
        );

        // Set configuration object
        if (false === $xmlConfig->replace($this->getXPathExtundelete(), $object))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED,
                $this->getXPathExtundelete()
            );
        }

        // Notify configuration changes
        $dispatcher = &OMVNotifyDispatcher::getInstance();
        $dispatcher->notify(OMV_NOTIFY_MODIFY, $this->getEventMessagePathExtundelete(), $object);
    }

    public function doRecoverExt($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Get the configuration object.
        $object = $xmlConfig->get( $this->getXPathExtundelete() );
        if (is_null($object))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXPathExtundelete()
            );
        }

        // Get paths
        $sourcePath = $object['source'];
        $destPath = OMVRpc::exec("ShareMgmt", "getPath", array("uuid" => $object['destination']), $context);

        // Create a background process.
        $bgStatusFilename = $this->createBgProcStatus();
        $pid = $this->fork();
        if ($pid > 0) { // Parent process.
            $this->initializeBgProcStatus($bgStatusFilename, $pid);
            return $bgStatusFilename;
        }

        // Child process.
        try {
            $bgOutputFilename = $this->createBgProcOutput();
            $this->updateBgProcStatus($bgStatusFilename, "outputfilename", $bgOutputFilename);

            $cmd = sprintf("cd %s && %s --restore-all %s 2>&1", $destPath, $this->getExtundeleteBin(), $sourcePath);

            $this->exec($cmd, $output, $bgOutputFilename);
            $this->finalizeBgProcStatus($bgStatusFilename, $output);
            exit(0);
        } catch(Exception $e) {
            $this->finalizeBgProcStatus($bgStatusFilename, "", $e);
            exit(1);
        }
    }

    function doSilentExt($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Get the configuration object.
        $object = $xmlConfig->get( $this->getXPathExtundelete() );
        if (is_null($object))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXPathExtundelete()
            );
        }

        // Get paths
        $sourcePath = $object['source'];
        $destPath = OMVRpc::exec("ShareMgmt", "getPath", array("uuid" => $object['destination']), $context);

        $cmd = sprintf("cd %s && %s --restore-all --log \"%sextundelete.log\" %s 2>&1", $destPath, $this->getExtundeleteBin(), $destPath, $sourcePath);

        if (0 !== $this->exec($cmd, $output))
        {
            throw new OMVException(
                OMVErrorMsg::E_EXEC_FAILED,
                $cmd,
                implode("\n", $output)
            );
        }
    }

    public function getRear($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Get configuration object
        $object = $xmlConfig->get( $this->getXPathRear() );
        if (is_null($object))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXPathRear()
            );
        }

        return $object;
    }

    public function setRear($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, '{
            "type"       : "object",
            "properties" : {
                "backuptype"         : { "type" : "string" },
                "incrementalday"     : { "type" : "string", "optional" : true },
                "output"             : { "type" : "string" },
                "fsuuid"             : { "type" : "string", "optional" : true  },
                "outputurltype"      : { "type" : "string", "optional" : true },
                "outputurl"          : { "type" : "string", "optional" : true },
                "outputusername"     : { "type" : "string", "optional" : true },
                "outputpassword"     : { "type" : "string", "optional" : true },
                "compressiontype"    : { "type" : "string" },
                "excludes"           : { "type" : "string" },
                "grubrescuepassword" : { "type" : "string" },
                "sshpassword"        : { "type" : "string" }
            }
        }');

        // Prepare configuration data
        $object = array(
            "backuptype"         => $params['backuptype'],
            "incrementalday"     => $params['incrementalday'],
            "output"             => $params['output'],
            "fsuuid"             => $params['fsuuid'],
            "outputurltype"      => $params['outputurltype'],
            "outputurl"          => $params['outputurl'],
            "outputusername"     => $params['outputusername'],
            "outputpassword"     => $params['outputpassword'],
            "compressiontype"    => $params['compressiontype'],
            "excludes"           => $params['excludes'],
            "grubrescuepassword" => $params['grubrescuepassword'],
            "sshpassword"        => $params['sshpassword']
        );

        // Set configuration object
        if (false === $xmlConfig->replace($this->getXPathRear(), $object))
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED,
                $this->getXPathRear()
            );
    }

    public function doRearBackup($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, '{
            "type"       : "object",
            "properties" : {
                "command" : { "type" : "string" }
            }
        }');

        if (!in_array($params['command'], self::$commands))
        {
            throw new OMVException(
                OMVErrorMsg::E_MISC_OPERATION_DENIED,
                sprinf("Unknown command - %s", $params['command'])
            );
        }

        // Create a background process.
        $bgStatusFilename = $this->createBgProcStatus();
        $pid = $this->fork();

        if ($pid > 0) { // Parent process.
            $this->initializeBgProcStatus($bgStatusFilename, $pid);
            return $bgStatusFilename;
        }

        // Child process.
        try {
            $bgOutputFilename = $this->createBgProcOutput();
            $this->updateBgProcStatus($bgStatusFilename, "outputfilename", $bgOutputFilename);

            $cmd = sprintf("omv-mkconf backup rear%s 2>&1", $params['command']);
            $this->exec($cmd, $output, $bgOutputFilename);

            $this->finalizeBgProcStatus($bgStatusFilename, $output);
            exit(0);
        } catch(Exception $e) {
            $this->finalizeBgProcStatus($bgStatusFilename, "", $e);
            exit(1);
        }
    }

    public function doDump($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Create a background process.
        $bgStatusFilename = $this->createBgProcStatus();
        $pid = $this->fork();
        if ($pid > 0) { // Parent process.
            $this->initializeBgProcStatus($bgStatusFilename, $pid);
            return $bgStatusFilename;
        }

        // Child process.
        try {
            $bgOutputFilename = $this->createBgProcOutput();
            $this->updateBgProcStatus($bgStatusFilename, "outputfilename", $bgOutputFilename);

            $cmd = sprintf("%s dump 2>&1", $this->getRearBin());

            $this->exec($cmd, $output, $bgOutputFilename);
            $this->finalizeBgProcStatus($bgStatusFilename, $output);
            exit(0);
        } catch(Exception $e) {
            $this->finalizeBgProcStatus($bgStatusFilename, "", $e);
            exit(1);
        }
    }
}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance();
$rpcServiceMgr->registerService(new OMVRpcServiceBackup());
