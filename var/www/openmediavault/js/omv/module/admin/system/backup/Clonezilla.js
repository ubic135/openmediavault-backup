/**
 * Copyright (C) 2013-2015 OpenMediaVault Plugin Developers
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
// require("js/omv/WorkspaceManager.js")
// require("js/omv/workspace/form/Panel.js")
// require("js/omv/workspace/window/Form.js")
// require("js/omv/data/Store.js")
// require("js/omv/data/Model.js")
// require("js/omv/data/proxy/Rpc.js")

Ext.define("OMV.module.admin.system.backup.Clonezilla", {
    extend : "OMV.workspace.form.Panel",
    uses   : [
        "OMV.data.Model",
        "OMV.data.Store"
    ],

    autoLoadData    : false,
    hideOkButton    : true,
    hideResetButton : true,
    mode            : "local",

    initComponent: function() {
        var me = this;
        OMV.Rpc.request({
            scope    : this,
            callback : function(id, success, response) {
                var parent = me.up('tabpanel');

                if (!parent)
                    return;

                var arch = response.arch;
                var n = arch.indexOf("arm");
                var panel = parent.down('panel[title=' + _("Clonezilla") + ']');
                if (panel) {
                    if (n < 0) {
                        panel.tab.show();
                    } else {
                        panel.tab.hide();
                    }
                }
                panel = parent.down('panel[title=' + _("SystemRescueCD") + ']');
                if (panel) {
                    if (n < 0) {
                        panel.tab.show();
                    } else {
                        panel.tab.hide();
                    }
                }
                panel = parent.down('panel[title=' + _("GParted Live") + ']');
                if (panel) {
                    if (n < 0) {
                        panel.tab.show();
                    } else {
                        panel.tab.hide();
                    }
                }
                panel = parent.down('panel[title=' + _("Parted Magic") + ']');
                if (panel) {
                    if (n < 0) {
                        panel.tab.show();
                    } else {
                        panel.tab.hide();
                    }
                }
                if (n > -1) {
                    panel = parent.down('panel[title=' + _("PhotoRec") + ']');
                    if (panel) {
                        parent.setActiveTab(panel);
                    }
                }
            },
            relayErrors : false,
            rpcData     : {
                service  : "OmvExtrasOrg",
                method   : "getArch"
            }
        });

        me.callParent(arguments);
    },

    getFormItems : function() {
        var me = this;
        return [{
            xtype    : "fieldset",
            title    : _("Clonezilla"),
            defaults : {
                labelSeparator : ""
            },
            items : [{
                xtype   : "button",
                name    : "install",
                text    : _("Install"),
                scope   : this,
                handler : Ext.Function.bind(me.onInstallButton, me, [ me ]),
                margin  : "5 0 0 0"
            },{
                border : false,
                html   : "<ul>" +
                         "<li>" + _("Downloads Clonezilla ISO and configures grub bootloader to allow booting from ISO.") + "</li>" +
                         "<li>" + _("SSH server is enabled by default.  Login with username: <b>user</b> and password: <b>live</b>") + "</li>" +
                         "<li>" + _("When connecting via ssh, the ssh key will be different than the OpenMediaVault ssh key and need to be updated on the client system.") + "</li>" +
                         "<li>" + _("IP Address will be set by DHCP.  Using static DHCP is recommended for headless servers.") + "</li>" +
                         "<li>" + _("When logging in remotely, start clonezilla with:  <b>sudo clonezilla</b>") + "</li>" +
                         "<li>" + _("ISO uses approximately 139 Mb in /boot directory on OS drive.") + "</li>" +
                         "</ul>"
            },{
                xtype   : "button",
                name    : "rebootcz",
                text    : _("Clonezilla"),
                scope   : this,
                handler : Ext.Function.bind(me.onCzButton, me, [ me ]),
                margin  : "5 0 0 0"
            },{
                border : false,
                html   : "<ul><li>" + _("Sets grub bootloader to boot from Clonezilla ISO <b>ONE</b> time.") + "</li></ul>"
            },{
                xtype   : "button",
                name    : "rebootomv",
                text    : _("OMV"),
                scope   : this,
                handler : Ext.Function.bind(me.onOmvButton, me, [ me ]),
                margin  : "5 0 0 0"
            },{
                border : false,
                html   : "<ul><li>" + _("Sets grub bootloader to boot normally from OpenMediaVault.") + "</li></ul>"
            }]
        }];
    },

    onInstallButton: function() {
        var me = this;
        var wnd = Ext.create("OMV.window.Execute", {
            title           : _("Install Clonezilla"),
            rpcService      : "Backup",
            rpcMethod       : "doInstallISO",
            rpcParams       : {
                command : "cz"
            },
            rpcIgnoreErrors : true,
            hideStartButton : true,
            hideStopButton  : true,
            listeners       : {
                scope     : me,
                finish    : function(wnd, response) {
                    wnd.appendValue(_("Done..."));
                    wnd.setButtonDisabled("close", false);
                },
                exception : function(wnd, error) {
                    OMV.MessageBox.error(null, error);
                    wnd.setButtonDisabled("close", false);
                }
            }
        });
        wnd.setButtonDisabled("close", true);
        wnd.show();
        wnd.start();
    },

    onCzButton: function() {
        var me = this;
        var wnd = Ext.create("OMV.window.Execute", {
            title           : _("Reboot to Clonezilla"),
            rpcService      : "Backup",
            rpcMethod       : "doRebootISO",
            rpcParams       : {
                command : "clonezilla"
            },
            rpcIgnoreErrors : true,
            hideStartButton : true,
            hideStopButton  : true,
            listeners       : {
                scope     : me,
                finish    : function(wnd, response) {
                    wnd.appendValue(_("Done..."));
                    wnd.setButtonDisabled("close", false);
                },
                exception : function(wnd, error) {
                    OMV.MessageBox.error(null, error);
                    wnd.setButtonDisabled("close", false);
                }
            }
        });
        wnd.setButtonDisabled("close", true);
        wnd.show();
        wnd.start();
    },

    onOmvButton: function() {
        var me = this;
        var wnd = Ext.create("OMV.window.Execute", {
            title           : _("Reboot to OpenMediaVault"),
            rpcService      : "Backup",
            rpcMethod       : "doRebootOMV",
            rpcIgnoreErrors : true,
            hideStartButton : true,
            hideStopButton  : true,
            listeners       : {
                scope     : me,
                finish    : function(wnd, response) {
                    wnd.appendValue(_("Done..."));
                    wnd.setButtonDisabled("close", false);
                },
                exception : function(wnd, error) {
                    OMV.MessageBox.error(null, error);
                    wnd.setButtonDisabled("close", false);
                }
            }
        });
        wnd.setButtonDisabled("close", true);
        wnd.show();
        wnd.start();
    }
});

OMV.WorkspaceManager.registerPanel({
    id        : "clonezilla",
    path      : "/system/backup",
    text      : _("Clonezilla"),
    position  : 20,
    className : "OMV.module.admin.system.backup.Clonezilla"
});
