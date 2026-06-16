<?php

namespace App\Policies;

use App\Models\ReferenceDatabase;
use App\Models\User;

class ReferenceDatabasePolicy
{
    public function viewAny(User $user): bool
    {
        return true;
    }

    public function view(User $user, ReferenceDatabase $database): bool
    {
        return true;
    }

    public function create(User $user): bool
    {
        return $user->isLabManager();
    }

    public function update(User $user, ReferenceDatabase $database): bool
    {
        return $user->isLabManager();
    }

    public function activate(User $user, ReferenceDatabase $database): bool
    {
        return $user->isLabManager();
    }
}
