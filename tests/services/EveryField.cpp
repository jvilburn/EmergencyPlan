#include "EveryField.h"
#include "CommandRegistry.h"
#include "CommandSamples.h"
#include "Crypto.h"

AccessList EveryField::access(const Identity& laptop, const Identity& phone)
{
    Identity desktop = Identity::generate();
    Identity bobInvite = Identity::generate();
    Identity carolInvite = Identity::generate();
    UserId alice = UserId::generate();
    UserId carol = UserId::generate();

    QList<AccessUser> users = {
        AccessUser(alice, "Alice Smith", AccessRole::Admin,
                   {AccessDevice("Laptop", laptop.publicKey(), laptop.verifyKey()),
                    AccessDevice("Pixel 8", phone.publicKey(), phone.verifyKey())}),
        AccessUser(carol, "Carol Jones", AccessRole::Member,
                   {AccessDevice("Desktop", desktop.publicKey(), desktop.verifyKey())}),
    };
    QList<AccessInvite> invites = {
        AccessInvite::forUser(alice, AccessRole::Member, "Bob", bobInvite.publicKey(), bobInvite.verifyKey()),
        AccessInvite::forDevice(carol, carolInvite.publicKey(), carolInvite.verifyKey()),
    };
    return AccessList(users, invites, {Crypto::randomBytes(Crypto::KeyBytes)});
}

DeviceState EveryField::deviceState(const AccessList& access)
{
    DeviceState device;
    for (const CommandType& type : CommandRegistry::types())
    {
        QList<CommandSample> samples = CommandSamples::forType(type.name);
        Q_ASSERT(!samples.isEmpty());
        device.journal.append(*samples.front().command);
    }
    device.baseVersion = 7;
    device.dismissedAccess = access;
    device.singleHolderNoticeDismissed = true;
    device.singleAdminNoticeDismissed = true;
    device.noLongerShared = true;
    device.inviteSeeds = {Crypto::randomBytes(Crypto::KeyBytes), Crypto::randomBytes(Crypto::KeyBytes)};
    return device;
}
