# Geographic Groups

## Purpose

Each family keeps a printed sheet of nearby families to contact for help in an emergency.
The ward is split into geographic groups of about eight families, and every family in a
group gets the same sheet listing the others. The groups are built automatically from the
families' map positions, saved as ward data, and adjusted by hand where straight-line
distance misjudges a neighborhood (a freeway or river between two houses).

This design builds on the shared ward's rules in `docs/SHARED_WARD_DESIGN.md`: *Replay*,
*References stay whole*, and the automatic updates under *Saving*.

## Decisions

- **Groups are saved, not recomputed.** A family moving in joins a group; the other groups
  stay as they are, so printed sheets stay valid.
- **The sheet carries what the ward directory already shows to every member**: family name,
  address, and each adult's phone and email.
- **An adult is anyone `isChild()` is false for**, which includes a person with no birth
  year on record.
- **Sheets are PDFs, one per group**, saved to a folder for emailing and for printing.
- **Running *Make Groups* again replaces every group**, hand moves included, after a
  confirmation. Undo restores the previous groups in one step.

## Data

The ward holds a list of geographic groups, each an id and a name. Each family holds an
optional group id.

```
ward:
  geographicGroups:
    - { id: <GeographicGroupId>, name: "Maple Ave" }
families:
  - id: <FamilyId>
    geographicGroup: <GeographicGroupId>   # absent when unassigned
```

Membership is a field on the family, so a family is in at most one group. Two users
offline moving one family to different groups make two changes to the same field: the
second conflicts and is handed back. A removed family takes its membership with it, so
the removal cleanup routine needs no entry for groups.

## Commands

These are journalled and replay like every other user command.

- **Make Groups** sets the whole group list and every family's group in one command. Its
  start condition is the group list and every family's group as the user saw them, so a
  concurrent move or rename makes it conflict. Unmapped families end it unassigned. Its
  undo restores the previous list and assignments, provided they are still what it wrote.
- **Set Geographic Group** is a per-field family update holding the group before and
  after. Its start condition includes that the target group exists, so a move replayed
  after a *Make Groups* replaced that group conflicts.
- **Rename Group** is a per-field update of the group's name.

## Automatic placement

A mapped family with no group joins the group of its nearest mapped family that had a
group when the pass started. The pass runs with the automatic updates, on the same
triggers (see *Automatic updates run on any device that holds the ward* in
`docs/SHARED_WARD_DESIGN.md`): after an import, after a committed command that gives a
family an address, and when a ward opens. It runs when the batch finishes, and also when
there is nothing to geocode, so a new family at an address the ward already has
coordinates for is placed too.

Placement is derived from the document as it stands, so it is an automatic update: it
applies and pushes, stays out of the journal, and when its push loses, the next pass after
the replay places the family again.

A family that already has a group keeps it, including after its address changes. With no
groups in the ward, the pass places nothing. A family stays unassigned while it is
unmapped; once groups exist, the pass places every mapped family, so the Unassigned
section holds the unmapped families.

## Building the groups

*Make Groups* asks for a target size, 8 by default.

1. Take the mapped families, with positions converted to meters around the ward's center,
   and set the group count to `ceil(families / target)`.
2. Split the families with a cut across the wider side of their spread, placed so each
   side's share of families matches its share of the groups: a set of 100 families for
   13 groups splits into 46 families for 6 groups and 54 for 7.
3. Repeat on each side until every part is one group.

Each cut rounds its share to whole families, so group sizes come out near even, and the
same input gives the same groups.

## Naming

A group is named after the street most of its families' first address lines name, with the
house number and any unit (`Apt 4`, `#12`, `Unit B`) removed. A tie goes to the street that
sorts first. When two groups would share a name, each is numbered in the order *Make
Groups* made them: `Maple Ave 1`, `Maple Ave 2`. A user can rename any group.

Assumed, to check against a real directory import: the street is always on the first
address line.

## Screens

### Geographic Groups tab

A tab in the Emergency Response sidebar.

- Toolbar: *Make Groups...*, *Move to...*, *Rename...*, *Save PDFs...*.
- A collapsible Unassigned section at the top, as in Ministering.
- Each group shows its name and family count. A group expands to its families, and a
  family to its adults with phone and email, as Teams shows members.
- Selecting a group highlights its families on the map.
- *Make Groups...* asks for the target size. When groups exist, it says that every group
  is replaced, hand moves included.
- *Move to...* lists the groups by distance to the selected family, nearest first, where a
  group's distance is that of its nearest mapped family. For an unmapped family, the list is
  alphabetical.

### Map

- While this tab is shown, markers are colored by group from a fixed palette. Colors are
  assigned in the order the splits made the groups, so neighboring groups usually differ.
- Right-clicking a family's marker opens a map context menu, on every tab. One item is
  *Set Geographic Group*, a submenu ordered as *Move to...* is, with the family's current
  group checked. The menu acts on the family a left-click at that point selects.

### Save PDFs...

The user picks a folder. Each group with families is written to `<group name>.pdf`,
replacing a file of that name, with characters Windows does not allow in file names
replaced by `-`.

Each sheet is titled *Emergency Contacts: \<group name\>* and lists the group's families
sorted by street and then by house number as a number. Each family shows its name and
address, then each adult's name, phone and email.

## Testing

In the existing Qt Test executable. Each test pins exact values.

**Building and naming**
- 20 families at fixed coordinates, target 8, make groups of 7, 7 and 6; the test pins each
  family's group, and a second run on the same input gives identical groups.
- An unmapped family stays unassigned while the mapped families in the same run are all
  placed.
- `123 Maple Ave`, `125 Maple Ave Apt 4` and `9 Oak St` name their group `Maple Ave`; a
  tie goes to the street that sorts first; two groups that would both be `Maple Ave`
  become `Maple Ave 1` and `Maple Ave 2`.

**Automatic placement**
- An imported family joins its nearest grouped neighbor's group, and so does a family at an
  address that already has coordinates and is not geocoded.
- A grouped family whose address changes keeps its group.
- With no groups, a mapped new family stays unassigned; the test asserts it is mapped.
- A placement whose push loses is placed again after the replay.

**Replay**
- Two offline moves of one family to different groups: the second conflicts, and the
  family is in exactly one group.
- *Make Groups* replayed after a hand move conflicts.
- A move to a group a later *Make Groups* replaced conflicts.
- Undoing *Make Groups* restores the previous groups and every family's group exactly.

**Sheets** (the generated page content)
- Adults are listed, a 10-year-old is not, and a person with no birth year is.
- `9 Oak St` sorts before `123 Oak St`.

## Open points

- **A group left empty by hand moves** stays in the list with a count of 0 until the next
  *Make Groups*; *Save PDFs* skips it, and placement never picks it, since it has no
  family to be nearest to. Deleting an empty group by hand is not part of this design.
