<?php

namespace App\Policies;

use App\Models\Run;
use App\Models\User;

class RunPolicy
{
    public function viewAny(User $user): bool
    {
        return true;
    }

    public function view(User $user, Run $run): bool
    {
        return $user->isLabManager() || $run->user_id === $user->id;
    }

    public function create(User $user): bool
    {
        return true;
    }

    public function delete(User $user, Run $run): bool
    {
        return $this->view($user, $run) && ! in_array($run->status, ['processing'], true);
    }
}
